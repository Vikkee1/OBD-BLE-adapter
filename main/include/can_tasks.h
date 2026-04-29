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

// OBD PIDs
#define PID_COUNT           5
#define RESPONSE_ID         0x7E8
#define REQUEST_ID          0x7DF

#define COOLANT_TEMP_PID    0x05
#define RPM_PID             0x0C
#define SPEED_PID           0x0D
#define ENGINE_LOAD_PID     0x04
#define FUEL_LEVEL_PID      0x2F

// COMMANDS
#define STOP_CMD            0x00
#define START_CMD           0x01
#define SUPP_PID_CMD        0x02
#define DTC_CMD             0x03
#define PID_CMD             0x10

// External handles
extern twai_node_handle_t node_hdl;
extern QueueHandle_t tx_queue;
extern QueueHandle_t rx_queue;

// Function prototypes
/**
 *  @param tx_io I/O for TX line 
 *  @param rx_io I/O for RX line
*/
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io);
void twai_tx_task(void *arg);
void twai_rx_task(void *arg);
void setup_tx_timer(uint64_t interval_ms);