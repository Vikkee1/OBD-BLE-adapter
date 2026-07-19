#include "can_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "message_bus.h"
#include "can_types.h"

/* Global handles */
twai_node_handle_t node_hdl = NULL;
QueueHandle_t tx_queue = NULL;

typedef struct {
    twai_frame_t frame;
    uint8_t      payload[8];
} tx_item_t;

enum Mode {
    IDLE,
    STREAM,
    SUPP_PIDS,
    DTC
};

static uint8_t requested_pids[PID_COUNT] = {RPM_PID, COOLANT_TEMP_PID, SPEED_PID, ENGINE_LOAD_PID, FUEL_LEVEL_PID};
static size_t pid_index = 0;

// ================= TWAI RX Callback =================
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    static uint8_t rx_buf[8];
    static twai_frame_t rx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };
    BaseType_t hpw = pdFALSE;

    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {

        /* Only forward OBD responses; drop everything else in the ISR. */
        if (rx_frame.header.id == RESPONSE_ID) {

            uint8_t dlc = rx_frame.header.dlc;
            if (dlc > sizeof(((bus_msg_t *)0)->frame.data)) {
                dlc = sizeof(((bus_msg_t *)0)->frame.data);
            }

            /* Copy the payload by value now — rx_buf is reused on every frame. */
            bus_msg_t msg = { .type = BUS_OBD_FRAME };
            msg.frame.id  = rx_frame.header.id;
            msg.frame.dlc = dlc;
            memcpy(msg.frame.data, rx_frame.buffer, dlc);

            bus_to_ble_post_from_isr(&msg, &hpw);
        }
    }

    /* Return the yield flag; the driver performs the context switch. */
    return hpw == pdTRUE;
}

// ================= TWAI TX Timer Callback =================
static void tx_timer_cb(void *arg) {

    tx_item_t item = {0};

    item.frame.header.id  = REQUEST_ID;
    item.frame.header.ide = 0;
    item.frame.header.rtr = 0;
    item.frame.buffer_len = 8;

    item.payload[0] = 0x02;
    item.payload[1] = 0x01;
    item.payload[2] = requested_pids[pid_index];
    memset(&item.payload[3], 0xAA, 5);

    xQueueSend(tx_queue, &item, 0);
    pid_index = (pid_index + 1) % PID_COUNT;
}

// ================= TWAI Initialization =================
esp_err_t init_TWAI(uint8_t tx_io, uint8_t rx_io){

    // Queues
    tx_queue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(tx_item_t));

    if (!tx_queue) return ESP_FAIL;

    // Node config
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = tx_io,             // TWAI TX GPIO pin
        .io_cfg.rx = rx_io,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 500000,   // 200 kbps bitrate
        .tx_queue_depth = 10,           // Transmit queue depth
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

    // Setup TX timer
    setup_tx_timer(100);

    return ESP_OK;
}

esp_err_t request_supported_pids(uint8_t pid){
    uint8_t rx_buf[8];
    twai_frame_t _tx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };

    _tx_frame.header.id = REQUEST_ID;
    _tx_frame.header.ide = 0;
    _tx_frame.header.rtr = 0;
    _tx_frame.buffer_len = 8;

    _tx_frame.buffer[0] = 0x02;
    _tx_frame.buffer[1] = 0x01;
    _tx_frame.buffer[2] = pid;

    memset(&_tx_frame.buffer[3], 0xAA, 5);

    return twai_node_transmit(node_hdl, &_tx_frame, pdMS_TO_TICKS(50));
}

esp_err_t request_dtc(){
    uint8_t rx_buf[8];
    twai_frame_t _tx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };

    _tx_frame.header.id = REQUEST_ID;
    _tx_frame.header.ide = 0;
    _tx_frame.header.rtr = 0;
    _tx_frame.buffer_len = 8;

    _tx_frame.buffer[0] = 0x02;
    _tx_frame.buffer[1] = 0x03;

    memset(&_tx_frame.buffer[2], 0xAA, 6);

    return twai_node_transmit(node_hdl, &_tx_frame, pdMS_TO_TICKS(50));
}

esp_err_t request_pid(uint8_t pid){
    uint8_t rx_buf[8];
    twai_frame_t _tx_frame = {
        .buffer = rx_buf,
        .buffer_len = sizeof(rx_buf),
    };

    _tx_frame.header.id = REQUEST_ID;
    _tx_frame.header.ide = 0;
    _tx_frame.header.rtr = 0;
    _tx_frame.buffer_len = 8;

    _tx_frame.buffer[0] = 0x02;
    _tx_frame.buffer[1] = 0x01;
    _tx_frame.buffer[2] = pid;

    memset(&_tx_frame.buffer[3], 0xAA, 5);

    return twai_node_transmit(node_hdl, &_tx_frame, pdMS_TO_TICKS(50));
}

// ================= TWAI TX Task =================
void twai_tx_task(void *arg) {
    
    enum Mode mode = IDLE;
    bus_msg_t from_ble_msg;
    static uint8_t _pids = 0, _cmd, _pid, prev_pid;

    ESP_LOGI(CAN_TAG, "CAN TX task created");

    while(1) {

        // UPDATE MODE
        if (bus_to_can_get(&from_ble_msg, 100)){

            _cmd = from_ble_msg.command.cmd;
            _pid = from_ble_msg.command.pid;

            ESP_LOGI(CAN_TAG, "CMD: %x %x", _cmd, _pid);

            if (_cmd == START_CMD){
                mode = STREAM;
            } else if(_cmd == SUPP_PID_CMD){
                mode = SUPP_PID_CMD;
            } else if(_cmd == DTC_CMD){
                mode = DTC;
            } else if (_cmd == PID_CMD){
                mode = PID_CMD;
            }else{
                mode = IDLE;
            }
        }

        // MODE
        if (mode == STREAM) {
            // Stream received data
            tx_item_t item;

            if (xQueueReceive(tx_queue, &item, portMAX_DELAY) == pdTRUE) {
                item.frame.buffer = item.payload;
                twai_node_transmit(node_hdl, &item.frame, pdMS_TO_TICKS(10));
            }

        }else if (mode == SUPP_PIDS){
            // Get supported PIDs
            request_supported_pids(_pids);
            _pids += 32;

            if (_pids > 160)
            {
                mode = IDLE;
                _pids = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(250));

        }else if ( mode == DTC) {
            request_dtc(); 
            mode = IDLE;

        }else if ( mode == PID_CMD){
            // Request commanded PID
            if (_pid != prev_pid) request_pid(_pid);
            prev_pid = _pid;
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
    esp_timer_start_periodic(timer, interval_ms * 1000);
}