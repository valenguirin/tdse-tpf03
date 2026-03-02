/*
 * act_led_amarillo.c
 *
 * Indicador visual de sesion BLE y errores de autenticacion.
 * El estado del LED se recalcula cada tick a partir del bus de eventos:
 *   - Apagado  : sin sesion activa.
 *   - Encendido: sesion BLE autenticada.
 *   - Parpadeo : credenciales incorrectas (100 ms de periodo durante 1 s).
 *
 * Los errores de autenticacion tienen prioridad sobre el estado de sesion.
 */
#include "act_led_amarillo.h"
#include "app_events.h"
#include "main.h"

typedef enum {
    ST_YEL_OFF,
    ST_YEL_ON,
    ST_YEL_BLINK
} fsm_led_amarillo_t;

static fsm_led_amarillo_t fsm_led_amarillo = ST_YEL_OFF;

void act_led_amarillo_init(void) {
    fsm_led_amarillo = ST_YEL_OFF;
    HAL_GPIO_WritePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin, GPIO_PIN_RESET);
}

void act_led_amarillo_update(void) {
    /* El estado se recalcula desde el bus en cada tick.
       Los errores de autenticacion tienen prioridad sobre la sesion abierta. */
    if (ev_ui_error_blink) {
        fsm_led_amarillo = ST_YEL_BLINK;
    } else if (ble_session_open) {
        fsm_led_amarillo = ST_YEL_ON;
    } else {
        fsm_led_amarillo = ST_YEL_OFF;
    }

    switch (fsm_led_amarillo) {
        case ST_YEL_OFF:
            HAL_GPIO_WritePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin, GPIO_PIN_RESET);
            break;

        case ST_YEL_ON:
            HAL_GPIO_WritePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin, GPIO_PIN_SET);
            break;

        case ST_YEL_BLINK:
            /* Alterna el pin cada 100 ms y desactiva el parpadeo al cumplir 1 s. */
            if ((HAL_GetTick() - ui_tick_toggle) >= 100U) {
                HAL_GPIO_TogglePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin);
                ui_tick_toggle = HAL_GetTick();
            }
            if ((HAL_GetTick() - ui_tick_error) >= 1000U) {
                ev_ui_error_blink = false;
            }
            break;
    }
}
