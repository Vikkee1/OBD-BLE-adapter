#include "can_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "obd_diag.h"

/* Global handles */
twai_node_handle_t node_hdl = NULL;

// ================= TWAI RX Callback ====================
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

    /* Return the yield flag; the driver performs the context switch. */
    return hpw == pdTRUE;
}

// ================= CAN state callback ==================
static bool twai_state_cb(twai_node_handle_t h, const twai_state_change_event_data_t *ed, void *ctx){
    ESP_EARLY_LOGW(CAN_TAG, "STATE %d -> %d",
                    ed->old_sta, ed->new_sta);
    return false;
}

// ================= CAN error callback ==================
static bool twai_err_cb(twai_node_handle_t h, const twai_error_event_data_t *ed, void *ctx){
    /* Fires on every frame - will flood */
    return false;
}

/* Public */

// ================= CAN Initialization =================
esp_err_t init_CAN(uint8_t tx_io, uint8_t rx_io){

    // Node config
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = tx_io,                     // TWAI TX GPIO pin
        .io_cfg.rx = rx_io,                     // TWAI RX GPIO pin
        .bit_timing.bitrate = BITRATE_500_KBPS, // bitrate
        .tx_queue_depth = TX_QUEUE_LENGTH,      // Transmit queue depth
    };

    twai_mask_filter_config_t mfilter_config = {
        .id     = 0x7E8,
        .mask   = 0x7F8,
        .is_ext = false,
    };

    twai_event_callbacks_t user_cbs = {
        .on_rx_done = twai_rx_cb,
        .on_error = twai_err_cb,
        .on_state_change = twai_state_cb
    };

    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    // Apply hardware filters
    ESP_ERROR_CHECK(twai_node_config_mask_filter(node_hdl, 0, &mfilter_config));

    // Register receive callback
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL));

    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    return ESP_OK;
}

// ================= Public send function ================
esp_err_t can_send(uint32_t id, const uint8_t payload[8], const uint8_t len){
    
    if (!node_hdl) return ESP_ERR_INVALID_STATE;

    uint8_t tx_buf[8];
    twai_frame_t tx_frame = {
        .header = {
            .id = id,
            .ide = 0,  // 0 = Standard (11-bit), 1 = Extended (29-bit)
            .rtr = 0,  // 0 = Data Frame, 1 = Remote Frame
            .dlc = len // Data Length Code (optional if buffer_len is set)
        },
        .buffer = tx_buf,
        .buffer_len = len,
    };

    memcpy(tx_frame.buffer, payload, len);

    return twai_node_transmit(node_hdl, &tx_frame, pdMS_TO_TICKS(10));
}