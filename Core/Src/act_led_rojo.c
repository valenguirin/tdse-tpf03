/*
 * act_led_rojo.c
 *
 * Indicador visual de operatividad del sistema.
 * El LED permanece encendido en standby y se apaga durante una emergencia
 * activa para indicar que el sistema esta ocupado procesando la alarma.
 */
#include "act_led_rojo.h"
#include "app_events.h"
#include "main.h"

typedef enum { ST_RED_OFF, ST_RED_ON } fsm_led_rojo_t;

static fsm_led_rojo_t fsm_led_rojo = ST_RED_ON;

void act_led_rojo_init(void) {
    fsm_led_rojo = ST_RED_ON;
    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);
}

void act_led_rojo_update(void) {
    switch (fsm_led_rojo) {
        case ST_RED_OFF:
            HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);
            if (cmd_led_rojo_on) {
                fsm_led_rojo    = ST_RED_ON;
                cmd_led_rojo_on = false;
            }
            break;

        case ST_RED_ON:
            HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);
            if (cmd_led_rojo_off) {
                fsm_led_rojo     = ST_RED_OFF;
                cmd_led_rojo_off = false;
            }
            break;
    }
}
