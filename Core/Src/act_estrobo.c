/*
 * act_estrobo.c
 *
 * Control de la baliza luminosa mediante una FSM de dos estados.
 * Se activa solo durante el modo nocturno, por orden del sistema central.
 * El apagado es automatico a los 20 s o por comando de silencio.
 */
#include "act_estrobo.h"
#include "app_events.h"
#include "main.h"

#define TIEMPO_ESTROBO_MS 20000U

typedef enum { ST_STROBE_OFF, ST_STROBE_ON } fsm_strobe_t;

static fsm_strobe_t fsm_estrobo  = ST_STROBE_OFF;
static uint32_t     tick_estrobo = 0;

void act_estrobo_init(void) {
    fsm_estrobo = ST_STROBE_OFF;
    HAL_GPIO_WritePin(ACT_ESTROBO_GPIO_Port, ACT_ESTROBO_Pin, GPIO_PIN_RESET);
}

void act_estrobo_update(void) {
    switch (fsm_estrobo) {
        case ST_STROBE_OFF:
            HAL_GPIO_WritePin(ACT_ESTROBO_GPIO_Port, ACT_ESTROBO_Pin, GPIO_PIN_RESET);
            if (cmd_strobe_on) {
                fsm_estrobo   = ST_STROBE_ON;
                tick_estrobo  = HAL_GetTick();
                cmd_strobe_on = false;
            }
            break;

        case ST_STROBE_ON:
            HAL_GPIO_WritePin(ACT_ESTROBO_GPIO_Port, ACT_ESTROBO_Pin, GPIO_PIN_SET);
            /* El apagado se produce por comando externo o por vencimiento del temporizador. */
            if (cmd_strobe_off || (HAL_GetTick() - tick_estrobo >= TIEMPO_ESTROBO_MS)) {
                fsm_estrobo    = ST_STROBE_OFF;
                cmd_strobe_off = false;
            }
            break;
    }
}
