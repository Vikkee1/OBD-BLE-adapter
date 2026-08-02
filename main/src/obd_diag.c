#include "obd_diag.h"
#include <string.h>

typedef struct {
    uint8_t  pid;
    uint8_t  resp_len;      /* payload bytes after the echoed PID */
    uint16_t period_ms;     /* filled in now, IGNORED this commit */
    float  (*decode)(const uint8_t *d);
    const char *name;
} pid_desc_t;

static float dec_rpm(const uint8_t *d)   { return ((d[0] << 8) | d[1]) / 4.0f; }
static float dec_speed(const uint8_t *d) { return d[0]; }
static float dec_coolant(const uint8_t *d) { return d[0] - 40.0f; }

static const pid_desc_t pid_table[] = {
    { 0x0C, 2,  50, dec_rpm,     "rpm"     },
    { 0x0D, 1, 100, dec_speed,   "speed"   },
    { 0x05, 1, 1000, dec_coolant, "coolant" },
};

#define PID_COUNT (sizeof(pid_table)/sizeof(pid_table[0]))

/*static const uint8_t requested_pids[PID_COUNT] = {
    RPM_PID, COOLANT_TEMP_PID, SPEED_PID, ENGINE_LOAD_PID, FUEL_LEVEL_PID
};*/

static const uint8_t supp_blocks[SUPP_BLOCK_COUNT] = {
    0x00, 0x20, 0x40, 0x60, 0x80, 0xA0
};

/* ============================================================
 * Time helpers. All arithmetic on uint32_t differences cast to
 * int32_t so the 49-day millisecond rollover is a non-event.
 * ============================================================ */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static inline bool reached(uint32_t deadline)
{
    return (int32_t)(now_ms() - deadline) >= 0;
}

static inline uint32_t ms_until(uint32_t deadline)
{
    int32_t d = (int32_t)(deadline - now_ms());
    return (d <= 0) ? 0u : (uint32_t)d;
}

static void send_flow_control(uint32_t resp_id)
{
    uint8_t p[8];
    memset(p, OBD_PAD_BYTE, sizeof(p));
    p[0] = 0x30;                 /* FC, ContinueToSend */
    p[1] = OBD_FC_BLOCK_SIZE;
    p[2] = OBD_FC_STMIN;

    /* Addressed to the ECU that answered, never to 0x7DF. */
    if (can_send(OBD_REQ_ID_FUNC, p, 8) != ESP_OK) {
        ESP_LOGW(OBD_TAG, "FC drop, tx_queue full");
    }
}

/* ============================================================
 * Single funnel for every request. Nothing else is allowed to
 * touch tx_queue with a request, so the pacing cannot be
 * bypassed by adding a new request_xxx() later.
 * ============================================================ */
static esp_err_t obd_start_request(obd_ctx_t *c, uint8_t service,
                                   uint8_t pid, bool has_pid)
{
    uint8_t p[8];
    memset(p, OBD_PAD_BYTE, sizeof(p));

    p[0] = has_pid ? 0x02 : 0x01;     /* SF, length */
    p[1] = service;
    if (has_pid) {
        p[2] = pid;
    }

    if (can_send(OBD_REQ_ID_FUNC, p, 8) != ESP_OK) {
        ESP_LOGE(OBD_TAG, "TX failed, retry next tick");
        return ESP_FAIL;          /* stay REQ_IDLE, poll gap still applies */
    }

    c->pending_service = service;
    c->pending_pid     = pid;
    c->pending_has_pid = has_pid;
    c->state           = REQ_WAIT_RESP;
    c->last_tx_ms      = now_ms();
    c->deadline_ms     = c->last_tx_ms + OBD_P2_TIMEOUT_MS;

    ESP_LOGI(OBD_TAG, "REQ svc=%02X pid=%02X", service, pid);
    return ESP_OK;
}

/* ============================================================
 * Transaction completion. This is what unblocks the next
 * request: state goes back to REQ_IDLE and the scheduler is
 * free to fire again once poll_gap_ms has elapsed.
 * ============================================================ */
static void obd_advance(obd_ctx_t *c)
{
    switch (c->mode) {
    case STREAM:
        c->pid_index = (c->pid_index + 1) % PID_COUNT;
        break;
    case SUPP_PIDS:
        if (++c->supp_index >= SUPP_BLOCK_COUNT) {
            c->supp_index = 0;
            c->mode = IDLE;           /* sweep finished */
        }
        break;
    case DTC:
    case ONE_SHOT:
    case VIN:
        c->mode = IDLE;               /* one-shot modes */
        break;
    default:
        break;
    }
}

static void obd_complete(obd_ctx_t *c, bool ok)
{
    c->state   = REQ_IDLE;
    c->retries = 0;
    c->asm_len = c->asm_got = 0;

    if (ok) {
        c->responses++;
        c->consecutive_failures = 0;
        c->poll_gap_ms = OBD_MIN_POLL_GAP_MS;
    } else if (++c->consecutive_failures >= (PID_COUNT * 2)) {
        /* Nothing is answering. Back off hard instead of hammering
         * the bus with a request every 20 ms forever. */
        if (c->poll_gap_ms != OBD_IDLE_BACKOFF_MS) {
            ESP_LOGW(OBD_TAG, "no responses, backing off to %d ms",
                     OBD_IDLE_BACKOFF_MS);
        }
        c->poll_gap_ms = OBD_IDLE_BACKOFF_MS;
    }

    obd_advance(c);
}

/* ============================================================
 * Deliver a completed service response upstream
 * ============================================================ */
static void obd_deliver(uint32_t id, const uint8_t *data, uint16_t len)
{
    /* bus_msg_t.frame carries 8 bytes. Anything longer is chunked;
     * see the notes on extending the bus message for VIN/DTC. */
    uint16_t off = 0;
    while (off < len) {
        uint8_t n = (len - off > 8) ? 8 : (uint8_t)(len - off);

        app_msg_t m = { .type = MSG_CAN_FRAME };
        m.frame.id  = id;
        m.frame.dlc = n;
        memcpy(m.frame.data, &data[off], n);

        if (!ble_mailbox_post(&m)) {
            ESP_LOGW(OBD_TAG, "BLE queue full");
        }
        off += n;
    }
}

/* ============================================================
 * Service payload match
 * ============================================================ */
static bool service_matches(const obd_ctx_t *c, const uint8_t *d, uint16_t len)
{
    if (len < 1) {
        return false;
    }
    if (d[0] != (uint8_t)(c->pending_service + 0x40)) {
        return false;
    }
    if (c->pending_has_pid) {
        return (len >= 2) && (d[1] == c->pending_pid);
    }
    return true;
}

/* Returns true if the transaction is closed by this payload. */
static void handle_service_payload(obd_ctx_t *c, uint32_t id,
                                   const uint8_t *d, uint16_t len)
{
    if (c->state != REQ_WAIT_RESP && c->state != REQ_WAIT_CF) {
        ESP_LOGD(OBD_TAG, "unsolicited resp %02X", d[0]);
        return;                                   /* nothing outstanding */
    }

    /* --- negative response --- */
    if (d[0] == 0x7F) {
        if (len < 3 || d[1] != c->pending_service) {
            return;                               /* someone else's NRC */
        }
        if (d[2] == 0x78) {
            /* responsePending: the ECU is asking for more time.
             * Extend the window, do NOT resend, do NOT advance. */
            c->deadline_ms = now_ms() + OBD_P2_STAR_TIMEOUT_MS;
            ESP_LOGD(OBD_TAG, "NRC 0x78, extending to P2*");
            return;
        }
        ESP_LOGW(OBD_TAG, "NRC %02X (svc %02X pid %02X)",
                 d[2], c->pending_service, c->pending_pid);
        obd_complete(c, false);
        return;
    }

    /* --- positive response, must match service AND pid --- */
    if (!service_matches(c, d, len)) {
        return;                                   /* not our pair, ignore */
    }

    obd_deliver(id, d, len);
    obd_complete(c, true);                        /* <-- releases next TX */
}

/* ============================================================
 * ISO-TP frame dispatch
 * ============================================================ */
static void obd_handle_frame(obd_ctx_t *c, const can_frame_t *f)
{
    if (f->id < OBD_RESP_ID_FIRST || f->id > OBD_RESP_ID_LAST || f->dlc == 0) {
        return;
    }

    uint8_t pci = f->data[0] >> 4;

    switch (pci) {

    case ISOTP_SF: {
        uint8_t len = f->data[0] & 0x0F;
        if (len == 0 || len > 7 || len + 1 > f->dlc) {
            return;
        }
        handle_service_payload(c, f->id, &f->data[1], len);
        break;
    }

    case ISOTP_FF: {
        if (c->state != REQ_WAIT_RESP || f->dlc < 8) {
            return;
        }
        uint16_t total = (uint16_t)((f->data[0] & 0x0F) << 8) | f->data[1];
        if (total < 7) {
            return;
        }
        /* Only accept the multiframe if its first bytes are our answer. */
        if (!service_matches(c, &f->data[2], 6)) {
            return;
        }

        c->asm_id      = f->id;
        c->asm_len     = (total > ISOTP_MAX_PAYLOAD) ? ISOTP_MAX_PAYLOAD : total;
        c->asm_got     = 6;
        c->asm_next_sn = 1;
        memcpy(c->asm_buf, &f->data[2], 6);

        send_flow_control(f->id);

        c->state       = REQ_WAIT_CF;
        c->deadline_ms = now_ms() + OBD_N_CR_TIMEOUT_MS;
        break;
    }

    case ISOTP_CF: {
        if (c->state != REQ_WAIT_CF || f->id != c->asm_id) {
            return;
        }
        uint8_t sn = f->data[0] & 0x0F;
        if (sn != c->asm_next_sn) {
            ESP_LOGW(OBD_TAG, "CF seq error: got %u want %u", sn, c->asm_next_sn);
            obd_complete(c, false);               /* abort, do not reassemble */
            return;
        }
        c->asm_next_sn = (uint8_t)((sn + 1) & 0x0F);

        uint16_t room = (uint16_t)(c->asm_len - c->asm_got);
        uint16_t n    = (f->dlc > 1) ? (uint16_t)(f->dlc - 1) : 0;
        if (n > room) {
            n = room;
        }
        memcpy(&c->asm_buf[c->asm_got], &f->data[1], n);
        c->asm_got = (uint16_t)(c->asm_got + n);

        if (c->asm_got >= c->asm_len) {
            obd_deliver(c->asm_id, c->asm_buf, c->asm_len);
            obd_complete(c, true);                /* <-- releases next TX */
        } else {
            c->deadline_ms = now_ms() + OBD_N_CR_TIMEOUT_MS;
        }
        break;
    }

    default:
        break;    /* FC from an ECU: we never send multiframe, ignore */
    }
}

/* ============================================================
 * Timeout supervision
 * ============================================================ */
static void obd_check_timeout(obd_ctx_t *c)
{
    if (c->state == REQ_IDLE || !reached(c->deadline_ms)) {
        return;
    }

    c->timeouts++;

    if (c->state == REQ_WAIT_CF) {
        /* Half a multiframe is worthless and the ECU may still be
         * sending. Drop it and let the poll gap cover us. */
        ESP_LOGW(OBD_TAG, "N_Cr timeout, %u/%u bytes", c->asm_got, c->asm_len);
        obd_complete(c, false);
        return;
    }

    if (++c->retries <= OBD_MAX_RETRIES) {
        ESP_LOGD(OBD_TAG, "P2 timeout pid %02X, retry %u",
                 c->pending_pid, c->retries);
        c->state = REQ_IDLE;              /* same PID, resent after the gap */
    } else {
        ESP_LOGW(OBD_TAG, "pid %02X gave up after %u tries",
                 c->pending_pid, c->retries);
        obd_complete(c, false);           /* skip to the next PID */
    }
}

/* ============================================================
 * Scheduler: the only place a request is born.
 * Two gates, both mandatory:
 *   1. no transaction outstanding
 *   2. at least poll_gap_ms since the last frame we put on the bus
 * ============================================================ */
static void obd_schedule(obd_ctx_t *c)
{
    if (c->state != REQ_IDLE) {
        return;                                        /* gate 1 */
    }
    if (!reached(c->last_tx_ms + c->poll_gap_ms)) {
        return;                                        /* gate 2 */
    }

    switch (c->mode) {
    case STREAM:
        obd_start_request(c, 0x01, pid_table[c->pid_index].pid, true);
        break;
    case SUPP_PIDS:
        obd_start_request(c, 0x01, supp_blocks[c->supp_index], true);
        break;
    case ONE_SHOT:
        obd_start_request(c, 0x01, c->oneshot_pid, true);
        break;
    case DTC:
        obd_start_request(c, 0x03, 0x00, false);
        break;
    case VIN:
        obd_start_request(c, 0x09, 0x02, true);
        break;
    case IDLE:
    default:
        break;
    }
}

/* ============================================================
 * BLE command handling
 * ============================================================ */
static void obd_handle_cmd(obd_ctx_t *c, uint8_t cmd, uint8_t pid)
{
    ESP_LOGI(OBD_TAG, "CMD %02X %02X", cmd, pid);

    switch (cmd) {
    case STOP_CMD:      c->mode = IDLE;                          break;
    case START_CMD:     c->mode = STREAM;    c->pid_index  = 0;  break;
    case SUPP_PID_CMD:  c->mode = SUPP_PIDS; c->supp_index = 0;  break;
    case DTC_CMD:       c->mode = DTC;                           break;
    case VIN_CMD:       c->mode = VIN;                           break;
    case PID_CMD:       c->mode = ONE_SHOT;  c->oneshot_pid = pid; break;
    default:
        ESP_LOGW(OBD_TAG, "unknown cmd %02X", cmd);
        return;
    }

    /* A mode change abandons whatever was outstanding, but the poll
     * gap still applies, so a spamming client cannot flood the bus. */
    c->state                = REQ_IDLE;
    c->retries              = 0;
    c->consecutive_failures = 0;
    c->poll_gap_ms          = OBD_MIN_POLL_GAP_MS;
}

/* ============================================================
 * How long may we sleep before something needs doing?
 * ============================================================ */
static uint32_t next_wakeup_ms(const obd_ctx_t *c)
{
    uint32_t w;

    if (c->mode == IDLE) {
        w = OBD_TASK_MAX_SLEEP_MS;
    } else if (c->state != REQ_IDLE) {
        w = ms_until(c->deadline_ms);                 /* P2 / N_Cr */
    } else {
        w = ms_until(c->last_tx_ms + c->poll_gap_ms); /* poll gap  */
    }

    return (w > OBD_TASK_MAX_SLEEP_MS) ? OBD_TASK_MAX_SLEEP_MS : w;
}

/* ============================================================
 * Task
 * ============================================================ */
void obd_request_task(void *arg)
{
    static obd_ctx_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.mode        = IDLE;
    ctx.state       = REQ_IDLE;
    ctx.poll_gap_ms = OBD_MIN_POLL_GAP_MS;

    ESP_LOGI(OBD_TAG, "OBD task started");

    app_msg_t msg;

    for (;;) {
        /* Blocks until a CAN frame or a BLE command arrives, or until
         * the next timer expires. A matching response therefore wakes
         * this task immediately -- no fixed 50/100/250 ms delays. */

        if (obd_mailbox_receive(&msg, OBD_MIN_POLL_GAP_MS)) {
            switch (msg.type) {
            case MSG_BLE_COMMAND:   obd_handle_cmd(&ctx, msg.command.cmd, msg.command.pid); break;
            case MSG_CAN_FRAME:  obd_handle_frame(&ctx, &msg.frame);                     break;
            default: break;
            }
        }

        obd_check_timeout(&ctx);
        obd_schedule(&ctx);
    }
}