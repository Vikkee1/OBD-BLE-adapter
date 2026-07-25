#pragma once

#include "esp_timer.h"
#include "message_bus.h"
#include "can_types.h"
#include "freertos/queue.h"
#include "esp_log.h"

// TAG
#define OBD_TAG "OBD"

// COMMANDS
#define STOP_CMD            0x00
#define START_CMD           0x01
#define SUPP_PID_CMD        0x02
#define DTC_CMD             0x03
#define PID_CMD             0x10

// OBD PIDs
#define PID_COUNT           5
#define RESPONSE_ID         0x7E8
#define REQUEST_ID          0x7DF

#define COOLANT_TEMP_PID    0x05
#define RPM_PID             0x0C
#define SPEED_PID           0x0D
#define ENGINE_LOAD_PID     0x04
#define FUEL_LEVEL_PID      0x2F

enum Mode {
    IDLE,
    STREAM,
    SUPP_PIDS,
    DTC
};

struct obd_request_ctxt
{
    tx_item_t last_received_item;
    tx_item_t last_transmitted_item;
};


void obd_request_task(void *arg);