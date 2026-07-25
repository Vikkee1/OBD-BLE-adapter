#include "obd_request_task.h"
#include <string.h>

static uint8_t requested_pids[PID_COUNT] = {RPM_PID, COOLANT_TEMP_PID, SPEED_PID, ENGINE_LOAD_PID, FUEL_LEVEL_PID};
static size_t pid_index = 0;
esp_timer_handle_t timer;
extern QueueHandle_t tx_queue;

enum SubStates{
    ON_ENTRY,
    RUN,
    DONE
};

// ================= DTC Request =================
esp_err_t request_dtc(){
    tx_item_t item = {0};

    item.frame.header.id  = REQUEST_ID;
    item.frame.header.ide = 0;
    item.frame.header.rtr = 0;
    item.frame.buffer_len = 8;

    item.payload[0] = 0x02;
    item.payload[1] = 0x03;
    memset(&item.payload[2], 0xAA, 6);

    if (xQueueSend(tx_queue, &item, 0) != pdTRUE) return ESP_FAIL;

    return ESP_OK;
}

// ================= OBD PID Request =================
esp_err_t request_pid(uint8_t pid){
    tx_item_t item = {0};

    item.frame.header.id  = REQUEST_ID;
    item.frame.header.ide = 0;
    item.frame.header.rtr = 0;
    item.frame.buffer_len = 8;

    item.payload[0] = 0x02;
    item.payload[1] = 0x01;
    item.payload[2] = pid;
    memset(&item.payload[3], 0xAA, 5);

    if (xQueueSend(tx_queue, &item, 0) != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

// ================= VIN Request =================
esp_err_t request_vin(){

    tx_item_t item = {0};

    item.frame.header.id  = REQUEST_ID;
    item.frame.header.ide = 0;
    item.frame.header.rtr = 0;
    item.frame.buffer_len = 8;

    item.payload[0] = 0x02;
    item.payload[1] = 0x09;
    item.payload[2] = 0x02;
    memset(&item.payload[3], 0xAA, 5);

    if (xQueueSend(tx_queue, &item, 0) != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

// ================= ISOTP Flow Control =================
esp_err_t send_flow_control(){
    tx_item_t item = {0};

    item.frame.header.id  = REQUEST_ID;
    item.frame.header.ide = 0;
    item.frame.header.rtr = 0;
    item.frame.buffer_len = 8;

    item.payload[0] = 0x30;
    memset(&item.payload[1], 0x00, 7);

    if (xQueueSend(tx_queue, &item, 0) != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

void mode_state_machine(enum Mode state){
    static enum SubStates sub_state = ON_ENTRY;

    switch (state){
        case STREAM:
            request_pid(requested_pids[pid_index]);
            pid_index = (pid_index + 1) % PID_COUNT;
            vTaskDelay(pdMS_TO_TICKS(100));

        case SUPP_PIDS:
            static uint8_t pids;

            switch (sub_state){
                
                case ON_ENTRY:
                    pids = 0;
                    sub_state = RUN;
                    break;

                case RUN:

                    if (pids > 160){
                        pids = 0;
                        sub_state = DONE;
                    }

                    request_pid(pids);
                    pids += 32;
                
                case DONE:
                    break;
                default:
                    break;
                }
            
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        
        case DTC:
            request_dtc();
            break;
        
        case IDLE:
            break;
        default:
            break;
    }
}

void obd_request_task(void *arg){

    enum Mode mode = IDLE;
    bus_msg_t from_ble_msg;
    static uint8_t  _pid;

    while (1)
    {
        // Subscribe to BLE commands
        if (bus_to_can_get(&from_ble_msg, 100)){

            mode = from_ble_msg.command.cmd;
            _pid = from_ble_msg.command.pid;

            ESP_LOGI(OBD_TAG, "CMD: %x %x", mode, _pid);
        }

        mode_state_machine(mode);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}