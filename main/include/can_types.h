#pragma once
#include <stdint.h>
#include "esp_twai.h"

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint32_t timestamp_ms;
    uint8_t  flags;
} can_frame_t;

typedef struct {
    twai_frame_t frame;
    uint8_t      payload[8];
} tx_item_t;