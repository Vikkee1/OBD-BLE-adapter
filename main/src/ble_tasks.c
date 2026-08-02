#include "ble_tasks.h"
#include "gatt.h"
#include "gap.h"
#include "message_bus.h"

static bool ble_send( const uint8_t *data, size_t len) {

    /* Check if connected and initialized */
    if (!is_connected()){
        return false;
    }
    
    /* Create mbuf with data */
    struct os_mbuf *om =
        ble_hs_mbuf_from_flat(data, len);

    if (!om) {
        ESP_LOGE(GATT_TAG, "Failed to allocate mbuf");
        return false;
    }

    /* Notify */
    ble_gatts_notify_custom(
        get_ble_conn_handle(),
        get_ble_val_handle(),
        om);

    return true;
}

static void ble_disconnect_handler(void){
    app_msg_t msg;

    /* On disconnect, tell the CAN side to stop streaming (cmd 0x00 -> IDLE). */
    msg.type = MSG_BLE_COMMAND;
    msg.command.cmd = 0x00;
    msg.command.pid = 0x00;

    if (!obd_mailbox_post(&msg)) {
        ESP_LOGW("BUS", "BLE->OBD queue full");
    }
}

static void ble_rx_handler(uint8_t *data, uint16_t len) {
    app_msg_t msg;

    /* BLE write carries a command: data[0] = cmd, data[1] = pid. */
    msg.type = MSG_BLE_COMMAND;
    msg.command.cmd = (len > 0) ? data[0] : 0x00;
    msg.command.pid = (len > 1) ? data[1] : 0x00;

    if (!obd_mailbox_post(&msg)) {
        ESP_LOGW("BUS", "BLE->CAN queue full");
    }
}

void ble_task_init(void){
    gatt_register_rx_callback(ble_rx_handler);
    gap_register_disc_callback(ble_disconnect_handler);
}

void ble_tx_task(void *param)
{
    ESP_LOGI(BLE_TASK_TAG, "BLE TX task started");

    app_msg_t msg;

    while (1) {

        if (ble_mailbox_receive(&msg, portMAX_DELAY)) {
            if (is_connected()) {
                if(!ble_send(msg.frame.data, msg.frame.dlc)){
                    ESP_LOGE(BLE_TASK_TAG, "SEND FAILED");
                };
            }
        }
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}