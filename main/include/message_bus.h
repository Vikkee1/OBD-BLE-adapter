#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"

typedef enum { BUS_CMD, BUS_OBD_FRAME, BUS_STATUS } bus_msg_type_t;

typedef struct {
    bus_msg_type_t type;
    union {
        struct { uint8_t cmd, pid; }                command;   /* to_can */
        struct { uint32_t id; uint8_t dlc, data[8]; } frame;   /* to_ble */
        struct { uint8_t code; }                    status;    /* either  */
    };
} bus_msg_t;

typedef struct __attribute__((packed)) {
    uint16_t rpm;
    uint8_t speed;
    uint8_t coolant_temp;
    uint8_t fuel_level;
} obd_data_t;

void message_bus_init(void);
bool bus_to_can_post(const bus_msg_t *m);            /* BLE side produces */
bool bus_to_can_get (bus_msg_t *m, uint32_t to_ms);  /* twai_tx_task consumes */
bool bus_to_ble_post(const bus_msg_t *m);            /* CAN side produces */
bool bus_to_ble_get (bus_msg_t *m, uint32_t to_ms);  /* ble_tx_task consumes */
bool bus_to_ble_post_from_isr(const bus_msg_t *msg, BaseType_t *higher_prio_woken);