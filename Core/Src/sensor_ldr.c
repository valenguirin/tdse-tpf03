/*
 * sensor_ldr.c
 *
 * FSM del sensor de luz LDR para deteccion del ciclo dia/noche.
 * Una sola lectura instantanea no es suficiente para cambiar el estado;
 * el pin debe mantenerse estable durante 2 segundos antes de confirmar
 * la transicion. Esto evita falsas alarmas por sombras o destellos.
 */

#include "sensor_ldr.h"
#include "app_events.h"
#include "main.h"
#include <stdbool.h>

typedef enum {
    ST_LDR_DAY,           /* Ambiente con luz, modo diurno activo. */
    ST_LDR_VERIFY_NIGHT,  /* Se detecto oscuridad, se espera confirmacion. */
    ST_LDR_NIGHT,         /* Oscuridad confirmada, modo nocturno activo. */
    ST_LDR_VERIFY_DAY     /* Se detecto luz, se espera confirmacion. */
} fsm_ldr_t;

static fsm_ldr_t fsm_ldr  = ST_LDR_DAY;
static uint32_t  tick_ldr = 0;

void sensor_ldr_init(void) {
    fsm_ldr  = ST_LDR_DAY;
    tick_ldr = 0;
}

void sensor_ldr_update(void) {
    bool is_dark = (HAL_GPIO_ReadPin(SENS_LDR_GPIO_Port, SENS_LDR_Pin) == GPIO_PIN_SET);

    switch (fsm_ldr) {

        case ST_LDR_DAY:
            if (is_dark) {
                fsm_ldr  = ST_LDR_VERIFY_NIGHT;
                tick_ldr = HAL_GetTick();
            }
            break;

        case ST_LDR_VERIFY_NIGHT:
            /* Si vuelve la luz antes de 2 s, la oscuridad fue momentanea. */
            if (!is_dark) {
                fsm_ldr = ST_LDR_DAY;
            } else if ((HAL_GetTick() - tick_ldr) >= 2000U) {
                fsm_ldr          = ST_LDR_NIGHT;
                ev_sys_night_mode = true;
            }
            break;

        case ST_LDR_NIGHT:
            if (!is_dark) {
                fsm_ldr  = ST_LDR_VERIFY_DAY;
                tick_ldr = HAL_GetTick();
            }
            break;

        case ST_LDR_VERIFY_DAY:
            if (is_dark) {
                fsm_ldr = ST_LDR_NIGHT;
            } else if ((HAL_GetTick() - tick_ldr) >= 2000U) {
                fsm_ldr        = ST_LDR_DAY;
                ev_sys_day_mode = true;
            }
            break;
    }
}
