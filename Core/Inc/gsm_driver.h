/**
 * @file  gsm_driver.h
 * @brief Driver de inicialización no bloqueante del módem SIM800L.
 *
 * Envía AT+CLIP=1 y AT+CMGF=1 mediante HAL_UART_Transmit_IT,
 * usando HAL_GetTick() para los temporizadores (sin HAL_Delay).
 */
#ifndef GSM_DRIVER_H
#define GSM_DRIVER_H

#include <stdbool.h>

void gsm_driver_init(void);
void gsm_driver_update(void);
bool gsm_driver_is_ready(void);      /* Retorna true cuando la inicialización completó. */

#endif /* GSM_DRIVER_H */
