#include "message_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MSG_BUS_TAG "MSG_BUS_TAG"
#define BUS_QUEUE_LEN 16

static QueueHandle_t to_ble_q;
static QueueHandle_t to_can_q;

void message_bus_init(void)
{
    to_ble_q = xQueueCreate(16, sizeof(bus_msg_t));
    to_can_q = xQueueCreate(16, sizeof(bus_msg_t));

    configASSERT(to_ble_q);
    configASSERT(to_can_q);
}

bool bus_to_ble_post(const bus_msg_t *msg)
{
    if (xQueueSend(to_ble_q, msg, 0) == pdTRUE) {
        return true;
    }
    
    bus_msg_t stale;
    xQueueReceive(to_ble_q, &stale, 0);
    return xQueueSend(to_ble_q, msg, 0) == pdTRUE;
}

bool bus_to_ble_post_from_isr(const bus_msg_t *msg, BaseType_t *higher_prio_woken)
{
    return xQueueSendFromISR(to_ble_q, msg, higher_prio_woken) == pdTRUE;
}

bool bus_to_ble_get(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(to_ble_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool bus_to_can_post(const bus_msg_t *msg)
{
    return xQueueSend(to_can_q, msg, 0) == pdTRUE;
}

bool bus_to_can_get(bus_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(to_can_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}