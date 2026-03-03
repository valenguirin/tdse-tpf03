/*
 * act_sirena.c
 *
 * Control del rele acustico mediante una FSM de dos estados.
 * El actuador se activa por comando del sistema central y se apaga
 * automaticamente a los 20 s o por comando de silencio mediante BLE.
 */
#include "act_sirena.h"
#include "app_events.h"
#include "main.h"

#define TIEMPO_SIRENA_MS 20000U

typedef enum { ST_SIREN_OFF, ST_SIREN_ON } fsm_siren_t;

static fsm_siren_t fsm_sirena  = ST_SIREN_OFF;
static uint32_t    tick_sirena = 0;

void act_sirena_init(void) {
    fsm_sirena = ST_SIREN_OFF;
    HAL_GPIO_WritePin(ACT_SIRENA_GPIO_Port, ACT_SIRENA_Pin, GPIO_PIN_RESET);
}

void act_sirena_update(void) {
    switch (fsm_sirena) {
        case ST_SIREN_OFF:
            HAL_GPIO_WritePin(ACT_SIRENA_GPIO_Port, ACT_SIRENA_Pin, GPIO_PIN_RESET);
            if (cmd_siren_on) {
                fsm_sirena   = ST_SIREN_ON;
                tick_sirena  = HAL_GetTick();
                cmd_siren_on = false;
            }
            break;

        case ST_SIREN_ON:
            HAL_GPIO_WritePin(ACT_SIRENA_GPIO_Port, ACT_SIRENA_Pin, GPIO_PIN_SET);
            /* El apagado se produce por comando externo o por vencimiento del temporizador. */
            if (cmd_siren_off || (HAL_GetTick() - tick_sirena >= TIEMPO_SIRENA_MS)) {
                fsm_sirena    = ST_SIREN_OFF;
                cmd_siren_off = false;
            }
            break;
    }
}
