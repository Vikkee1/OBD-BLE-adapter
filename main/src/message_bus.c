#include "message_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MSG_BUS_TAG "MSG_BUS_TAG"
#define MSG_QUEUE_LEN 16

static QueueHandle_t ble_q;
static QueueHandle_t obd_q;

void mailbox_init(void)
{
    ble_q = xQueueCreate(MSG_QUEUE_LEN, sizeof(app_msg_t));
    obd_q = xQueueCreate(MSG_QUEUE_LEN, sizeof(app_msg_t));

    configASSERT(ble_q);
    configASSERT(obd_q);

}

bool ble_mailbox_post(const app_msg_t *msg)
{
    return xQueueSend(ble_q, msg, 0) == pdTRUE;
}

bool ble_mailbox_receive(app_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(ble_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool obd_mailbox_post(const app_msg_t *msg)
{
    return xQueueSend(obd_q, msg, 0) == pdTRUE;
}

bool obd_mailbox_post_from_isr(const app_msg_t *msg, BaseType_t *higher_prio_woken)
{
    return xQueueSendFromISR(obd_q, msg, higher_prio_woken) == pdTRUE;
}

bool obd_mailbox_receive(app_msg_t *msg, uint32_t timeout_ms)
{
    return xQueueReceive(obd_q,
                         msg,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}