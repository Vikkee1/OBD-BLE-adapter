#ifndef WS2812_H
#define WS2812_H

#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define HIGH    1
#define LOW     0

#define WS2812_T0H  4   // 400 ns
#define WS2812_T0L  9   // 900 ns
#define WS2812_T1H  8   // 800 ns
#define WS2812_T1L  5   // 500 ns
#define WS2812_RESET_US 50

/* Function signatures */
void ws2812_init(uint8_t led_io);
void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b);

#endif