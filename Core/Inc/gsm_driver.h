/* Inicializacion del modem SIM800L sin bloquear el loop */
#ifndef GSM_DRIVER_H
#define GSM_DRIVER_H

#include <stdbool.h>

void gsm_driver_init(void);
void gsm_driver_update(void);
bool gsm_driver_is_ready(void);      /* Retorna true cuando la inicialización completó. */

#endif /* GSM_DRIVER_H */
