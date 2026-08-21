/* STD APIs */
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

/* ESP APIs */
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "nvs.h"

/* FreeRTOS APIs */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"

/* CAN task APIs */
#include "can_tasks.h"
#include "obd_diag.h"

/* BLE APIs*/
#include "ble_stack.h"
#include "ble_tasks.h"

/* Message bus layer */
#include "message_bus.h"

/* Misc */
#include "ws2812.h"

/* I/O configuration - board/target specific, see Kconfig.projbuild
 * and sdkconfig.defaults.<target> */
#define IO_TX       CONFIG_OBD_CAN_TX_GPIO
#define IO_RX       CONFIG_OBD_CAN_RX_GPIO
#define WS2812_GPIO CONFIG_OBD_LED_GPIO

#define APP_TAG "APP"
#define USB_TASK_PERIOD (50 / portTICK_PERIOD_MS)

esp_err_t init_nvs(void){
    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_LOGD(APP_TAG, "New NVS page");
        ret = nvs_flash_init();
    }

    if(ret != ESP_OK){
        ESP_LOGE(APP_TAG, "failed to initialize nvs flash, error code: %d ", ret);
    }
   
    return ret;
}

void app_main(void)
{

    /* Initialize NVS */
    if (init_nvs() != ESP_OK) {
        return;
    }

    /* Initialize message bus */
    mailbox_init();

    /* Initialize CAN*/
    init_CAN(IO_TX, IO_RX);

    /* Initialize BLE stack */
    ble_stack_init();

    /* Initialize GAP */
    gap_init();

    /* Initialize GATT */
    gatt_svc_init();

    ble_task_init();

    /* Start BLE stack*/
    ble_stack_start();

    /* Start OBD scheduler */
    xTaskCreate(obd_request_task, "OBD task", 2*2048, NULL, 3, NULL);

    /* Start BLE task */
    xTaskCreate(ble_tx_task, "BLE TX task", 4*1024, NULL, 5, NULL);
    
}