#include "message_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MSG_BUS_TAG "MSG_BUS_TAG"
#define BUS_QUEUE_LEN 16

static QueueHandle_t from_ble_q;
static QueueHandle_t from_can_q;

void message_bus_init(void)
{
    from_ble_q = xQueueCreate(16, sizeof(bus_msg_t));
    from_can_q = xQueueCreate(16, sizeof(bus_msg_t));

    configASSERT(from_ble_q);
    configASSERT(from_can_q);
}

bool bus_publish_ble(const bus_msg_t *msg)
{
    return xQueueSend(from_ble_q, msg, 0) == pdTRUE;
}

bool bus_publish_can(const bus_msg_t *msg)
{
    return xQueueSend(from_can_q, msg, 0) == pdTRUE;
}

bool bus_subscribe_ble(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(from_ble_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool bus_subscribe_can(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(from_can_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}