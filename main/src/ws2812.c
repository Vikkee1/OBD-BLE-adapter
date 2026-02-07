#include "ws2812.h"

static rmt_channel_handle_t rmt_chan;
static rmt_encoder_handle_t encoder;

static size_t ws2812_encode(
    const void *data,
    size_t data_size,
    size_t symbols_written,
    size_t symbols_free,
    rmt_symbol_word_t *symbols,
    bool *done,
    void *arg
) {
    const uint8_t *bytes = data;
    size_t symbol_pos = 0;
    size_t bit_pos = symbols_written;

    for (size_t i = bit_pos; i < data_size * 8 && symbol_pos < symbols_free; i++) {
        uint8_t byte = bytes[i / 8];
        bool bit = byte & (1 << (7 - (i % 8)));

        if (bit) {
            symbols[symbol_pos++] = (rmt_symbol_word_t){
                .level0 = 1,
                .duration0 = WS2812_T1H,
                .level1 = 0,
                .duration1 = WS2812_T1L,
            };
        } else {
            symbols[symbol_pos++] = (rmt_symbol_word_t){
                .level0 = 1,
                .duration0 = WS2812_T0H,
                .level1 = 0,
                .duration1 = WS2812_T0L,
            };
        }
    }

    if (bit_pos + symbol_pos >= data_size * 8) {
        *done = true;
    }

    return symbol_pos;
}

void ws2812_init(uint8_t led_io)
{
    rmt_tx_channel_config_t tx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = led_io,
        .mem_block_symbols = 64,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .trans_queue_depth = 4,
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &rmt_chan));
    ESP_ERROR_CHECK(rmt_enable(rmt_chan));

    rmt_simple_encoder_config_t encoder_config = {
        .callback = ws2812_encode,
        .arg = NULL,
    };

    ESP_ERROR_CHECK(rmt_new_simple_encoder(&encoder_config, &encoder));

    ESP_LOGI("WS2812", "Initialized via RMT");
}

void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,   // no looping
    };
    uint8_t data[3] = { g, r, b }; // WS2812 = GRB
    rmt_transmit(rmt_chan, encoder, data, sizeof(data), &tx_cfg);
    rmt_tx_wait_all_done(rmt_chan, portMAX_DELAY);
}
