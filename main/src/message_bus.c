#include "message_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MSG_BUS_TAG "MSG_BUS_TAG"
#define CAN_BUS_QUEUE_LEN 16

static QueueHandle_t can_bus_q;

void can_bus_init(void){
    can_bus_q = xQueueCreate(CAN_BUS_QUEUE_LEN, sizeof(can_frame_t));
    configASSERT(can_bus_q);
}

bool can_bus_publish(const can_frame_t *frame){
    return xQueueSend(can_bus_q, frame, 0) == pdTRUE;
}

bool can_bus_subscribe(can_frame_t *frame, uint32_t timeout_ms){
    return xQueueReceive(can_bus_q, frame, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}