#ifndef GATT_SVR
#define GATT_SVR

/* NimBLE GATT APIs */
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* NimBLE GAP APIs */
#include "host/ble_gap.h"

/* TAG*/
#define GATT_TAG "GATT"

typedef void (*gatt_rx_callback_t)(uint8_t *data, uint16_t len);

/* Public function declarations */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);
int gatt_svc_init(void);
uint16_t get_ble_conn_handle(void);
uint16_t get_ble_val_handle(void);
bool is_connected(void);
void gatt_register_rx_callback(gatt_rx_callback_t cb);

#endif 