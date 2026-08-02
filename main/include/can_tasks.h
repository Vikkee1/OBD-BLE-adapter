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
extern QueueHandle_t can_tx_queue;
extern QueueHandle_t can_rx_queue;

typedef enum {
    CAN_TX_NORMAL,
    CAN_TX_URGENT     /* flow control — N_Bs is tight, jump the queue */
} can_tx_priority_t;

// Function prototypes
/**
 *  @param tx_io I/O for TX line 
 *  @param rx_io I/O for RX line
*/

/** -
 *  @return ESP_OK if ,
 *          ESP_ERR_NO_MEM */
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io);

/** Q--
 *  @return ESP_OK ,
 *          ESP_ERR_NO_MEM */
void twai_tx_task(void *arg);

/** Queue a classic 8-byte CAN frame for transmission.
 *  @return ESP_OK if accepted into the TX queue (not yet on the wire),
 *          ESP_ERR_NO_MEM if the queue is full. */
esp_err_t can_transmit(uint32_t id, const uint8_t payload[8],
                       can_tx_priority_t prio);