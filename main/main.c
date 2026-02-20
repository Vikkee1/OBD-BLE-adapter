/* STD APIs */
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
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

/* CAN APIs */
#include "can_tasks.h"

/* BLE APIs*/
#include "ble_stack.h"
#include "ble_tasks.h"

/* Transport & message layer */
#include "message_bus.h"
#include "transport.h"

/* Misc */
#include "ws2812.h"

/* I/O configuration */
#define IO_TX       5
#define IO_RX       4
#define WS2812_GPIO 48

#define APP_TAG "APP"
#define USB_TASK_PERIOD (50 / portTICK_PERIOD_MS)

void usb_tx_task(void *param){
    /* Task entry log */
    ESP_LOGI(APP_TAG, "USB task has been started!");

    can_frame_t frame;

    /* Loop forever */
    while (1) {

        if ( can_bus_subscribe(&frame, 100) ){
            printf("ID: %lx DATA: %x %x %x %x \n", 
                frame.id, frame.data[0], frame.data[1], frame.data[2], frame.data[3]);
        }

        /* Sleep */
        vTaskDelay(USB_TASK_PERIOD);
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}

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

    /* InitializeTWAI*/
    init_TWAI(IO_TX, IO_RX);

    /* Initialize NVS */
    if (init_nvs() != ESP_OK) {
        return;
    }

    /* Initialize BLE stack */
    ble_stack_init();

    /* Initialize GAP */
    gap_init();

    /* Initialize GATT */
    gatt_svc_init();

    /* Start LED */
    //ws2812_init(WS2812_GPIO);
    //ws2812_set_color(0, 5, 0);

    /* Start BLE stack*/
    ble_stack_start();

    /* Initialize transport layer */
    transport_init();

    /* Initialize message bus */
    can_bus_init();

    /* Create CAN RX and TX tasks. Setup TX timer. */
    xTaskCreatePinnedToCore(twai_tx_task, "TWAI_TX", 2*2048, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(twai_rx_task, "TWAI_RX", 2*2048, NULL, 3, NULL, 1);

    /* Start BLE task */
    xTaskCreate(ble_tx_task, "BLE TX task", 4*1024, NULL, 5, NULL);

    /* USB debug task */
    //xTaskCreate(usb_tx_task, "USB_RX", 2048, NULL, 5, NULL);
    return;
}