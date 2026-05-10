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
    bus_msg_t msg;

    msg.id = 0x01;
    msg.len = 0;

    if (!bus_publish_ble(&msg)) {
        ESP_LOGW("BUS", "BLE->CAN queue full");
    }
}

static void ble_rx_handler(uint8_t *data, uint16_t len) {
    bus_msg_t msg;

    msg.id = 0x01;
    msg.len = len;

    memcpy(msg.data, data, len);

    if (!bus_publish_ble(&msg)) {
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

    bus_msg_t msg;

    while (1) {

        if (bus_subscribe_can(&msg, portMAX_DELAY)) {

            if (is_connected()) {
                if(!ble_send(msg.data, msg.len)){
                    ESP_LOGW(GATT_TAG, "SEND FAILED");
                };
            }
        }
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}