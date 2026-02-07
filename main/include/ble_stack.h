#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "gap.h"
#include "gatt.h"

#define BLE_TAG "BLE"

void ble_stack_init(void);
void ble_stack_start(void);