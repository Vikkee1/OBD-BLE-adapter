#pragma once

#include "can_types.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_ENCODE_LEN 13

size_t can_encode(can_frame_t *f, uint8_t *out, size_t max_len);