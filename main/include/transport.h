#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Transport backend function signature
 * Must be non-blocking or bounded-blocking
 */
typedef bool (*transport_send_fn_t)(const uint8_t *data, size_t len);

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
 * Send data through all registered transports
 * Thread-safe, non-blocking
 */
bool transport_send(const uint8_t *data, size_t len);