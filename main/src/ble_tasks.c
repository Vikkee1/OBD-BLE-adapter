#include "ble_tasks.h"
#include "gatt.h"
#include "transport.h"
#include "message_bus.h"

static bool ble_transport_send(transport_source_t source, const uint8_t *data, size_t len) {

    /* Ignore if source is BLE */
    if (source == TRANSPORT_SRC_BLE) {
        return true;
    }

    /* Check if connected and initialized */
    if (!is_connected()){
        return false;
    }
    
    /* Create mbuf with data */
    struct os_mbuf *om =
        ble_hs_mbuf_from_flat(data, sizeof(data));

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

static bool ble_send( const uint8_t *data, size_t len) {

    /* Check if connected and initialized */
    if (!is_connected()){
        return false;
    }
    
    /* Create mbuf with data */
    struct os_mbuf *om =
        ble_hs_mbuf_from_flat(data, sizeof(data));

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

void ble_task_transport_init(void){
    transport_register(ble_transport_send);
    ESP_LOGI(BLE_TASK_TAG, "BLE transport registered!");
}

void ble_tx_task(void *param)
{
    ESP_LOGI(BLE_TASK_TAG, "BLE TX task started");

    bus_msg_t msg;

    while (1) {

        if (bus_subscribe_ble(&msg, portMAX_DELAY)) {

            if (is_connected()) {
                ble_send(msg.data, msg.len);
            }
        }
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}