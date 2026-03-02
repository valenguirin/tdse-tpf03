/**
 * @file  sensor_gsm.h
 * @brief FSM de detección y validación de llamadas GSM entrantes.
 */
#ifndef SENSOR_GSM_H
#define SENSOR_GSM_H

void sensor_gsm_init(void);
void sensor_gsm_update(void);
void sensor_gsm_rx_callback(void);    /* Llamar desde HAL_UART_RxCpltCallback (USART3). */
void sensor_gsm_error_callback(void); /* Llamar desde HAL_UART_ErrorCallback (USART3).  */

#endif /* SENSOR_GSM_H */
