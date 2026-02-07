#include "codec.h"

size_t can_encode(can_frame_t *f, uint8_t *out, size_t max_len){
    if (max_len < MAX_ENCODE_LEN) return 0;
    return 0;
}