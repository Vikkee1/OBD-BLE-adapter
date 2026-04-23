#include "ble_tasks.h"
#include "gatt.h"
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

void ble_tx_task(void *param)
{
    ESP_LOGI(BLE_TASK_TAG, "BLE TX task started");

    bus_msg_t msg;

    while (1) {

        if (bus_subscribe_ble(&msg, portMAX_DELAY)) {

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