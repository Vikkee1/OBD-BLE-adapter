#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "can_types.h"

typedef enum {
    MSG_BLE_COMMAND,    /* BLE CMD: {cmd, pid}        */
    MSG_CAN_FRAME,      /* CAN: raw response frame */
    MSG_OBD_RESULT,     /* decoded/reassembled OBD payload  */
    MSG_STATUS          /* timeout, mode change, error  */
} app_msg_type_t;

typedef struct {
    app_msg_type_t type;
    union {
        struct { uint8_t cmd, pid; }    command;    /* command */
        can_frame_t                     frame;      /* data frame */
        struct { uint32_t id; uint16_t len; uint8_t data[64]; } result;
        struct { uint8_t code; }        status;     /* either  */
    };
} app_msg_t;

void mailbox_init(void);
bool obd_mailbox_post(const app_msg_t *msg);
bool obd_mailbox_post_from_isr(const app_msg_t *msg, BaseType_t *hpw);
bool obd_mailbox_receive(app_msg_t *msg, uint32_t timeout_ms);

bool ble_mailbox_post(const app_msg_t *msg);
bool ble_mailbox_receive(app_msg_t *msg, uint32_t timeout_ms);