/*
 * sensor_boton.c
 *
 * FSM del boton de panico fisico.
 * El pin se lee por polling cada 1 ms desde el ejecutor ciclico.
 * La FSM tiene cuatro estados para filtrar el rebote mecanico del contacto.
 */

#include "sensor_boton.h"
#include "app_events.h"
#include "main.h"

/* Tiempo minimo que el pin debe permanecer en nivel bajo para confirmar
   una pulsacion real. Valores menores se descartan como ruido. */
#define DELAY_BOTON_MS 50U

typedef enum {
    ST_BTN_PANIC_UP,       /* Pin en nivel alto, boton sin presionar. */
    ST_BTN_PANIC_FALLING,  /* Se detecto flanco de bajada, inicia el filtro. */
    ST_BTN_PANIC_DOWN,     /* Pulsacion confirmada, evento enviado al bus. */
    ST_BTN_PANIC_RISING    /* Pin vuelve a nivel alto, espera estabilidad. */
} fsm_sensor_btn_t;

static fsm_sensor_btn_t fsm_btn  = ST_BTN_PANIC_UP;
static uint32_t         tick_btn = 0;

void sensor_boton_init(void) {
    fsm_btn  = ST_BTN_PANIC_UP;
    tick_btn = 0;
}

void sensor_boton_update(void) {
    GPIO_PinState lectura = HAL_GPIO_ReadPin(SENS_BOTON_PANICO_GPIO_Port, SENS_BOTON_PANICO_Pin);

    switch (fsm_btn) {

        case ST_BTN_PANIC_UP:
            /* Al detectar flanco de bajada se guarda el tiempo para el filtro. */
            if (lectura == GPIO_PIN_RESET) {
                fsm_btn  = ST_BTN_PANIC_FALLING;
                tick_btn = HAL_GetTick();
            }
            break;

        case ST_BTN_PANIC_FALLING:
            /* Si el pin vuelve a nivel alto antes de los 50 ms, era ruido. */
            if ((HAL_GetTick() - tick_btn) >= DELAY_BOTON_MS) {
                if (lectura == GPIO_PIN_RESET) {
                    fsm_btn = ST_BTN_PANIC_DOWN;
                    ev_panico_boton = true;  /* Notifica al sistema central. */
                } else {
                    fsm_btn = ST_BTN_PANIC_UP;
                }
            }
            break;

        case ST_BTN_PANIC_DOWN:
            /* Espera que el usuario suelte el boton. */
            if (lectura == GPIO_PIN_SET) {
                fsm_btn  = ST_BTN_PANIC_RISING;
                tick_btn = HAL_GetTick();
            }
            break;

        case ST_BTN_PANIC_RISING:
            /* Aplica el mismo filtro en la liberacion para evitar doble disparo. */
            if ((HAL_GetTick() - tick_btn) >= DELAY_BOTON_MS) {
                if (lectura == GPIO_PIN_SET) {
                    fsm_btn = ST_BTN_PANIC_UP;
                } else {
                    fsm_btn = ST_BTN_PANIC_DOWN;
                }
            }
            break;
    }
}
