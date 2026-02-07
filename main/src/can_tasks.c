#include "can_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "can_types.h"
#include "transport.h"
#include "message_bus.h"

/* Global handles */
twai_node_handle_t node_hdl = NULL;
QueueHandle_t tx_queue = NULL;
QueueHandle_t rx_queue = NULL;
obd_data_t received_data = {0};

static uint8_t requested_pids[PID_COUNT] = {RPM_PID, COOLANT_TEMP_PID};
static size_t pid_index = 0;

// ================= TWAI RX Callback =================
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    static uint8_t rx_buf[8];
    static twai_frame_t rx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };

    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {
        
        can_frame_t frame;

        frame.id = rx_frame.header.id;
        frame.dlc = rx_frame.header.dlc;
        memcpy(frame.data, rx_frame.buffer, frame.dlc);

        xQueueSendFromISR(rx_queue, &frame, NULL);
    }
    return false;
}

// ================= TWAI TX Timer Callback =================
static void tx_timer_cb(void *arg) {
    can_frame_t frame;
    uint8_t pid = (uint8_t)(uintptr_t)arg;

    frame.id = 0x7DF;
    frame.dlc = 8;
    frame.data[0] = 0x02;
    frame.data[1] = 0x01;
    frame.data[2] = requested_pids[pid_index];

    memset(&frame.data[3], 0, 5);

    xQueueSendFromISR(tx_queue, &frame, NULL);

    // Move to next PID
    pid_index = (pid_index + 1) % PID_COUNT;
}

// ================= TWAI Initialization =================
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io){

    // Queues
    tx_queue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(can_frame_t));
    rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(can_frame_t));

    if (!tx_queue || !rx_queue) return ESP_FAIL;

    // Node config
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = tx_io,             // TWAI TX GPIO pin
        .io_cfg.rx = rx_io,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 250000,   // 200 kbps bitrate
        .tx_queue_depth = 10,           // Transmit queue depth set to 5
    };

    twai_event_callbacks_t user_cbs = {
        .on_rx_done = twai_rx_cb,
    };

    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    // Register receive callback
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL));

    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    //Start TX timer
    setup_tx_timer(500);

    return ESP_OK;
}

// ================= TWAI RX Task =================
void twai_rx_task(void *arg) {
    can_frame_t frame;
    ESP_LOGI(CAN_TAG, "CAN RX task created");

    while(1) {
        if (xQueueReceive(rx_queue, &frame, portMAX_DELAY) == pdTRUE) {
            if (frame.id == RESPONSE_ID) {
                can_bus_publish(&frame);
            }
        }
    }
}

// ================= TWAI TX Task =================
void twai_tx_task(void *arg) {
    can_frame_t frame;
    twai_frame_t tx_frame;
    static uint8_t buf[8];
    
    ESP_LOGI(CAN_TAG, "CAN TX task created");

    while(1) {
        if (xQueueReceive(tx_queue, &frame, portMAX_DELAY) == pdTRUE) {
            
            tx_frame.header.id = frame.id;
            tx_frame.header.ide = 0;
            tx_frame.header.rtr = 0;
            tx_frame.buffer_len = frame.dlc;
            
            memcpy(buf, frame.data, frame.dlc);
            tx_frame.buffer = buf;

            twai_node_transmit(node_hdl, &tx_frame, pdMS_TO_TICKS(10));
        }
    }
}

// ================= TWAI TX Timer setup =================
void setup_tx_timer(uint64_t interval_ms) {
    esp_timer_create_args_t timer_args = {
        .callback = &tx_timer_cb,
        .name = "TX_Timer"
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, interval_ms * 1000); // 250 ms
}