#include "transport.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

/* ================= Configuration ================= */

#define TRANSPORT_MAX_PAYLOAD     64
#define TRANSPORT_QUEUE_LENGTH   16
#define TRANSPORT_MAX_BACKENDS    4

/* ================= Internal Types ================= */

typedef struct {
    size_t  len;
    uint8_t data[TRANSPORT_MAX_PAYLOAD];
} transport_msg_t;

/* ================= Internal State ================= */

static transport_send_fn_t backends[TRANSPORT_MAX_BACKENDS];
static size_t backend_count;

static QueueHandle_t     tx_queue;
static SemaphoreHandle_t backend_mutex;

/* ================= Forward Decls ================= */

static void transport_tx_task(void *arg);

/* ================= Public API ================= */

void transport_init(void)
{
    backend_count = 0;

    backend_mutex = xSemaphoreCreateMutex();
    configASSERT(backend_mutex);

    tx_queue = xQueueCreate(
        TRANSPORT_QUEUE_LENGTH,
        sizeof(transport_msg_t)
    );
    configASSERT(tx_queue);

    xTaskCreatePinnedToCore(
        transport_tx_task,
        "transport_tx",
        4096,
        NULL,
        3,
        NULL,
        1   /* Core 1: application side */
    );
}

bool transport_register(transport_send_fn_t send_fn)
{
    if (!send_fn) {
        return false;
    }

    bool ok = false;

    xSemaphoreTake(backend_mutex, portMAX_DELAY);

    if (backend_count < TRANSPORT_MAX_BACKENDS) {
        backends[backend_count++] = send_fn;
        ok = true;
    }

    xSemaphoreGive(backend_mutex);
    return ok;
}

bool transport_send(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > TRANSPORT_MAX_PAYLOAD) {
        return false;
    }

    transport_msg_t msg;
    msg.len = len;
    memcpy(msg.data, data, len);

    /* Non-blocking send */
    return xQueueSend(tx_queue, &msg, 0) == pdTRUE;
}

/* ================= TX Task ================= */

static void transport_tx_task(void *arg)
{
    transport_msg_t msg;

    while (1) {
        if (xQueueReceive(tx_queue, &msg, portMAX_DELAY)) {

            xSemaphoreTake(backend_mutex, portMAX_DELAY);

            for (size_t i = 0; i < backend_count; i++) {
                backends[i](msg.data, msg.len);
            }

            xSemaphoreGive(backend_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}