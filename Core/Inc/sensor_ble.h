/**
 * @file  sensor_ble.h
 * @brief FSM de autenticación Bluetooth y gestión de comandos BLE.
 */
#ifndef SENSOR_BLE_H
#define SENSOR_BLE_H

void sensor_ble_init(void);
void sensor_ble_update(void);
void sensor_ble_rx_callback(void);    /* Llamar desde HAL_UART_RxCpltCallback (USART1). */
void sensor_ble_error_callback(void); /* Llamar desde HAL_UART_ErrorCallback (USART1).  */

#endif /* SENSOR_BLE_H */
