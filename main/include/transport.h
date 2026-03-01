#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TRANSPORT_SRC_BLE = 0,
    TRANSPORT_SRC_CAN,
    TRANSPORT_SRC_UNKNOWN
} transport_source_t;

/*
 * Transport backend function signature
 * Must be non-blocking or bounded-blocking
 */
typedef bool (*transport_send_fn_t)(transport_source_t source, const uint8_t *data, size_t len);

/*
 * Initialize transport system
 * Must be called once at startup
 */
void transport_init(void);

/*
 * Register a transport backend (BLE, WiFi, USB, Logger, ...)
 * Can be called at runtime
 */
bool transport_register(transport_send_fn_t send_fn);

/*
 * Send data to registered transports
 * Thread-safe, non-blocking
 */
bool transport_send(transport_source_t source,
                    const uint8_t *data,
                    size_t len);