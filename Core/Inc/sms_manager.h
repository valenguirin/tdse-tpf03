/**
 * @file  sms_manager.h
 * @brief Motor asíncrono de SMS con cola circular y TX no bloqueante.
 *
 * Usa HAL_UART_Transmit_IT para envíos; el flag_gsm_tx_done se actualiza
 * desde sms_manager_tx_done_callback() (llamar desde HAL_UART_TxCpltCallback).
 */
#ifndef SMS_MANAGER_H
#define SMS_MANAGER_H

void sms_manager_init(void);
void sms_manager_update(void);
void sms_manager_tx_done_callback(void);   /* Llamar desde HAL_UART_TxCpltCallback. */
void sms_manager_encolar(const char *num, const char *msg);
void sms_manager_generar_ruta(const char *tipo_activacion);

#endif /* SMS_MANAGER_H */
