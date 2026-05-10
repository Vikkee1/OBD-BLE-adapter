# _BLUETOOTH OBD READER_
This is implementation of OBD protocol reader. Application connects trough BLE to phone or other bluetooth device. Phone app for reading and displaying the data is separete project of mine.

## Application

The application is divided into dedicated FreeRTOS tasks:

- BLE stack task
    - Handles BLE stack initialization and runtime.
    - BLE functionality is implemented using the Apache NimBLE stack provided by ESP-IDF.

- GAP task
    - Manages BLE advertising, connection handling, and GAP events.

- GATT server task
    - Provides BLE services and characteristics.

- CAN tasks
    - Handle CAN transmission and reception independently.

Communication between BLE and CAN subsystems is implemented using a queue-based message bus. Both subsystems can publish and subscribe to messages asynchronously.

Adapter
![alt text](20260506_194049.jpg)

## How to use example
- Requirements
    - ESP-IDF environment installed
    - BLE-capable ESP32 device (ESP with BLE stack)
        - ESP32-S3 preferred 
    - CAN transceiver connected to the target hardware

- Build
    
    > 	`idf.py build`

- Flash
    > 	`idf.py flash monitor`