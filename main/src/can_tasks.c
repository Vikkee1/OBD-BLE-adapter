#include "can_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "can_types.h"
#include "obd_diag.h"

/* Global handles */
twai_node_handle_t node_hdl = NULL;
QueueHandle_t can_tx_queue = NULL;
QueueHandle_t can_rx_queue = NULL;

// ================= TWAI RX Callback =================
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    static uint8_t rx_buf[8];
    static twai_frame_t rx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };
    BaseType_t hpw = pdFALSE;

    app_msg_t msg;

    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {

        /* Only forward OBD responses; drop everything else in the ISR. */
        if (rx_frame.header.id > OBD_RESP_ID_FIRST ||
            rx_frame.header.id < OBD_RESP_ID_LAST) {

            uint8_t dlc = rx_frame.header.dlc;
            if (dlc > sizeof(((app_msg_t *)0)->frame.data)) {
                dlc = sizeof(((app_msg_t *)0)->frame.data);
            }

            msg.type = MSG_CAN_FRAME;
            msg.frame.id = rx_frame.header.id;
            msg.frame.dlc = dlc;
        
            memcpy(msg.frame.data, rx_frame.buffer, dlc);

            obd_mailbox_post_from_isr(&msg, &hpw);
        }
    }

    /* Return the yield flag; the driver performs the context switch. */
    return hpw == pdTRUE;
}

// ================= TWAI Initialization =================
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io){

    // Queues
    can_tx_queue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(can_frame_t));

    if (!can_tx_queue) return ESP_FAIL;

    // Node config
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = tx_io,             // TWAI TX GPIO pin
        .io_cfg.rx = rx_io,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 500000,   // 200 kbps bitrate
        .tx_queue_depth = 10,           // Transmit queue depth
    };

    twai_event_callbacks_t user_cbs = {
        .on_rx_done = twai_rx_cb,
    };

    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    // Register receive callback
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL));

    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    return ESP_OK;
}

esp_err_t can_transmit(uint32_t id, const uint8_t payload[8],
                       can_tx_priority_t prio)
{
    if (!node_hdl || !can_tx_queue) return ESP_ERR_INVALID_STATE;

    can_frame_t frame;
    frame.id  = id;
    frame.dlc = 8;
    memcpy(frame.data, payload, 8);

    BaseType_t ok = (prio == CAN_TX_URGENT)
                  ? xQueueSendToFront(can_tx_queue, &frame, 0)
                  : xQueueSend(can_tx_queue, &frame, 0);

    return (ok == pdTRUE) ? ESP_OK : ESP_ERR_NO_MEM;
}

// ================= TWAI TX Task =================
void twai_tx_task(void *arg) {
    
    ESP_LOGI(CAN_TAG, "CAN TX task created");

    can_frame_t bus_frame;
    static uint8_t tx_buf[8];
    static twai_frame_t tx_frame = {
        .buffer = tx_buf,
        .buffer_len = sizeof(tx_buf),
    };

    tx_frame.header.ide = 0;
    tx_frame.header.rtr = 0;

    for(;;) {

        /* Block until at least one frame is queued — yields the CPU. */
        if (xQueueReceive(can_tx_queue, &bus_frame, portMAX_DELAY) == pdTRUE) {

            tx_frame.header.id = bus_frame.id;
            tx_frame.buffer_len = bus_frame.dlc;
            
            memcpy(tx_frame.buffer, bus_frame.data, bus_frame.dlc);

            esp_err_t err = twai_node_transmit(node_hdl, &tx_frame, pdMS_TO_TICKS(10));
            if (err != ESP_OK) {
                ESP_LOGW(CAN_TAG, "TX failed: %s", esp_err_to_name(err));
            }
        }
    }
}