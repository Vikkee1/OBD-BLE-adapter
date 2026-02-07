#pragma once

#include "can_types.h"
#include <stdbool.h>

void can_bus_init(void);
bool can_bus_publish(const can_frame_t *frame);
bool can_bus_subscribe(can_frame_t *frame, uint32_t timeout_ms);