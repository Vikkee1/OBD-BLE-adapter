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

#define BITRATE_500_KBPS    500000

// External handles
extern twai_node_handle_t node_hdl;


/**
 *  @param tx_io I/O for TX line 
 *  @param rx_io I/O for RX line
*/

// Function prototypes

/** -
 *  @return ESP_OK if ,
 *          ESP_ERR_NO_MEM */
esp_err_t init_CAN(uint8_t tx_io, uint8_t rx_io);

/** Queue a classic 8-byte CAN frame for transmission.
 *  @return ESP_OK if send OK
 *          ESP_ERR if the send failed. */
esp_err_t can_send(uint32_t id, const uint8_t payload[8], const uint8_t len);