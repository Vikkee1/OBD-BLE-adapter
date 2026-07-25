#include "can_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "message_bus.h"
#include "can_types.h"

/* Global handles */
twai_node_handle_t node_hdl = NULL;
QueueHandle_t tx_queue = NULL;


// ================= TWAI RX Callback =================
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    static uint8_t rx_buf[8];
    static twai_frame_t rx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };
    BaseType_t hpw = pdFALSE;

    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {

        /* Only forward OBD responses; drop everything else in the ISR. */
        if (rx_frame.header.id > 0x700) {

            uint8_t dlc = rx_frame.header.dlc;
            if (dlc > sizeof(((bus_msg_t *)0)->frame.data)) {
                dlc = sizeof(((bus_msg_t *)0)->frame.data);
            }

            /* Copy the payload by value now — rx_buf is reused on every frame. */
            bus_msg_t msg = { .type = BUS_OBD_FRAME };
            msg.frame.id  = rx_frame.header.id;
            msg.frame.dlc = dlc;
            memcpy(msg.frame.data, rx_frame.buffer, dlc);

            bus_to_ble_post_from_isr(&msg, &hpw);
        }
    }

    /* Return the yield flag; the driver performs the context switch. */
    return hpw == pdTRUE;
}

// ================= TWAI Initialization =================
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io){

    // Queues
    tx_queue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(tx_item_t));

    if (!tx_queue) return ESP_FAIL;

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

// ================= TWAI TX Task =================
void twai_tx_task(void *arg) {
    
    ESP_LOGI(CAN_TAG, "CAN TX task created");

    tx_item_t item;

    while (1) {

        /* Block until at least one frame is queued — yields the CPU. */
        if (xQueueReceive(tx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        do {
            item.frame.buffer = item.payload;
            esp_err_t err = twai_node_transmit(node_hdl, &item.frame, pdMS_TO_TICKS(10));
            if (err != ESP_OK) {
                ESP_LOGW(CAN_TAG, "TX failed: %s", esp_err_to_name(err));
            }
        } while (xQueueReceive(tx_queue, &item, 0) == pdTRUE);   /* drain backlog */
    }

}