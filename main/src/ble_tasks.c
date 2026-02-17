#include "ble_tasks.h"
#include "gatt.h"
#include "transport.h"
#include "message_bus.h"

static bool ble_transport_send(const uint8_t *data, size_t len) {

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

    //ESP_LOGI(GATT_TAG, "Notification sent");

    return true;
}

void ble_task_transport_init(void){
    transport_register(ble_transport_send);
    ESP_LOGI(BLE_TASK_TAG, "BLE transport registered!");
}

void ble_tx_task(void *param){
    /* Task entry log */
    ESP_LOGI(BLE_TASK_TAG, "BLE TX task has been started!");

    uint8_t _obd_chr_val[3] = {0};

    static can_frame_t frame = {.data[0] = 0x9, .dlc = 1};
    uint8_t buffer[32];

    ble_task_transport_init();

    /* Loop forever */
    while (1) {

        if ( can_bus_subscribe(&frame, 100) ){
            
            transport_send(frame.data, frame.dlc);
        }

        /* Sleep */
        vTaskDelay(TASK_PERIOD);
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}