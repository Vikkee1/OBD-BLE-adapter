#pragma once

#include "can_types.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint32_t id;
    size_t len;
    uint8_t data[64];
} bus_msg_t;

typedef struct __attribute__((packed)) {
    uint16_t rpm;
    uint8_t speed;
    uint8_t coolant_temp;
    uint8_t fuel_level;
} obd_data_t;

void message_bus_init(void);
bool bus_publish_ble(const bus_msg_t *msg);
bool bus_publish_can(const bus_msg_t *msg);
bool bus_subscribe_ble(bus_msg_t *msg, uint32_t timeout_ms);
bool bus_subscribe_can(bus_msg_t *msg, uint32_t timeout_ms);