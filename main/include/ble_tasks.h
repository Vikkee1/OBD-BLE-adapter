#pragma once

#include <stdio.h>
#include "gatt.h"
#include "freertos/FreeRTOS.h"

#define BLE_TASK_TAG "BLE_TASK"
#define TASK_PERIOD (50 / portTICK_PERIOD_MS)

void ble_tx_task(void *param);
void ble_task_init(void);