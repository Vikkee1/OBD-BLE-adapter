#include "message_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MSG_BUS_TAG "MSG_BUS_TAG"
#define BUS_QUEUE_LEN 16

static QueueHandle_t ble_to_can_q;
static QueueHandle_t can_to_ble_q;

void message_bus_init(void)
{
    ble_to_can_q = xQueueCreate(16, sizeof(bus_msg_t));
    can_to_ble_q = xQueueCreate(16, sizeof(bus_msg_t));

    configASSERT(ble_to_can_q);
    configASSERT(can_to_ble_q);
}

bool bus_publish_ble(const bus_msg_t *msg)
{
    return xQueueSend(ble_to_can_q, msg, 0) == pdTRUE;
}

bool bus_publish_can(const bus_msg_t *msg)
{
    return xQueueSend(can_to_ble_q, msg, 0) == pdTRUE;
}

bool bus_subscribe_ble(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(can_to_ble_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool bus_subscribe_can(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(ble_to_can_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}