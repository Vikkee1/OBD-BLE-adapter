#pragma once

#include <stdint.h>
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TWAI_TAG    "TWAI"
#define CAN_TAG     "CAN"

#define TX_QUEUE_LENGTH     10
#define RX_QUEUE_LENGTH     10

// External handles
extern twai_node_handle_t node_hdl;
extern QueueHandle_t tx_queue;
static TaskHandle_t tx_task_handle; 

// Function prototypes
/**
 *  @param tx_io I/O for TX line 
 *  @param rx_io I/O for RX line
*/
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io);
void twai_tx_task(void *arg);