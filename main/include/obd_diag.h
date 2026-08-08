#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "message_bus.h"
#include "can_types.h"
#include "can_tasks.h"

#define OBD_TAG "OBD"

/* ============================================================
 * BLE command byte (data[0] of the GATT write)
 * ============================================================ */
#define STOP_CMD            0x00
#define START_CMD           0x01
#define SUPP_PID_CMD        0x02
#define DTC_CMD             0x03
#define PID_CMD             0x10   /* one-shot: data[1] = pid */
#define VIN_CMD             0x11

/* ============================================================
 * Addressing (ISO 15765-4, 11 bit)
 * ============================================================ */
#define OBD_REQ_ID_FUNC     0x7DF   /* functional broadcast request      */
#define OBD_PHYS_REQ_BASE   0x7E0   /* physical request  = base + n      */
#define OBD_RESP_ID_FIRST   0x7E8   /* physical response = first + n     */
#define OBD_RESP_ID_LAST    0x7EF

/* Physical request address of the ECU that sent response id `r` */
#define OBD_PHYS_REQ_OF(r)  (OBD_PHYS_REQ_BASE + ((r) - OBD_RESP_ID_FIRST))

/* ============================================================
 * ISO-TP
 * ============================================================ */
#define ISOTP_SF            0x0
#define ISOTP_FF            0x1
#define ISOTP_CF            0x2
#define ISOTP_FC            0x3

#define ISOTP_MAX_PAYLOAD   128     /* VIN=20, DTC list can be longer     */
#define OBD_PAD_BYTE        0xAA    /* keep whatever the branch used      */

/* Flow-control parameters we advertise to the ECU.
 * BS   = 0  -> "send every CF, no further FC needed"
 * STmin= 0  -> "as fast as you like". Raise (e.g. 0x0A = 10 ms) if the
 *              driver starts dropping consecutive frames. */
#define OBD_FC_BLOCK_SIZE   0x00
#define OBD_FC_STMIN        0x00

/* ============================================================
 * Timing  -- the core of the request pacing
 * ============================================================ */
#define OBD_P2_TIMEOUT_MS       100   /* P2_CAN is 50 ms; 100 = margin      */
#define OBD_P2_STAR_TIMEOUT_MS  5000  /* extended window after NRC 0x78     */
#define OBD_N_CR_TIMEOUT_MS     150   /* max gap between consecutive frames */
#define OBD_MIN_POLL_GAP_MS     50    /* hard floor between two requests    */
#define OBD_IDLE_BACKOFF_MS     1000  /* gap used when the bus looks dead   */
#define OBD_MAX_RETRIES         2
#define OBD_TASK_MAX_SLEEP_MS   100

/* ============================================================
 * PIDs
 * ============================================================ */
#define COOLANT_TEMP_PID    0x05
#define RPM_PID             0x0C
#define SPEED_PID           0x0D
#define ENGINE_LOAD_PID     0x04
#define FUEL_LEVEL_PID      0x2F

#define MAX_PID_COUNT       10

#define SUPP_BLOCK_COUNT    6       /* 0x00,0x20,0x40,0x60,0x80,0xA0 */

/* ============================================================
 * Modes / transaction state
 * ============================================================ */
enum Mode {
    IDLE = 0,
    STREAM,
    SUPP_PIDS,
    DTC,
    ONE_SHOT,
    VIN
};

typedef enum {
    REQ_IDLE = 0,     /* nothing outstanding, only the poll gap gates TX */
    REQ_WAIT_RESP,    /* request on the wire, P2 timer armed             */
    REQ_WAIT_CF       /* ISO-TP multiframe in progress, N_Cr timer armed */
} obd_req_state_t;

typedef struct {
    /* scheduling */
    enum Mode mode;
    size_t    pid_index;
    size_t    supp_index;
    uint8_t   oneshot_pid;
    uint32_t next_due[MAX_PID_COUNT];

    /* outstanding transaction */
    obd_req_state_t state;
    uint8_t  pending_service;
    uint8_t  pending_pid;
    bool     pending_has_pid;
    uint8_t  retries;

    /* timers, milliseconds, monotonic */
    uint32_t last_tx_ms;
    uint32_t deadline_ms;
    uint32_t poll_gap_ms;

    /* ISO-TP reassembly */
    uint32_t asm_id;
    uint16_t asm_len;
    uint16_t asm_got;
    uint8_t  asm_next_sn;
    uint8_t  asm_buf[ISOTP_MAX_PAYLOAD];

    /* health counters */
    uint32_t consecutive_failures;
    uint32_t timeouts;
    uint32_t responses;
} obd_ctx_t;

/* ============================================================
 * Inbound event queue (BLE commands + CAN frames in one place,
 * so the task has exactly one blocking point)
 * ============================================================ */
typedef enum { OBD_EVT_CMD, OBD_EVT_FRAME } obd_evt_type_t;

typedef struct {
    obd_evt_type_t type;
    union {
        struct { uint8_t cmd, pid; } command;
        can_frame_t                  frame;
    };
} obd_evt_t;

esp_err_t obd_request_task_init(void);
bool      obd_evt_post(const obd_evt_t *e);
bool      obd_evt_post_from_isr(const obd_evt_t *e, BaseType_t *hpw);
void      obd_request_task(void *arg);