/*
 * gsm_driver.c
 *
 * Inicializacion del modem SIM800L sin bloquear el loop.
 * Espera 3s a que se registre en la red, manda AT+CLIP=1 y AT+CMGF=1
 * con pausas de 500ms. Cuando termina, gsm_driver_is_ready() devuelve true.
 * Los envios van por Transmit_IT, sin HAL_Delay.
 */

#include "gsm_driver.h"
#include "app_events.h"
#include "main.h"
#include "usart.h"

/* Buffers estaticos: deben existir mientras dure la transmision IT. */
static const uint8_t CMD_CLIP[] = "AT+CLIP=1\r\n";
static const uint8_t CMD_CMGF[] = "AT+CMGF=1\r\n";

typedef enum {
    GSM_INIT_WAIT_NETWORK,    /* Espera inicial de 3 s para registro en red. */
    GSM_INIT_SEND_CLIP,       /* Inicia envio de AT+CLIP=1. */
    GSM_INIT_WAIT_CLIP_TX,    /* Espera confirmacion de TX. */
    GSM_INIT_WAIT_CLIP_DELAY, /* Pausa de 500 ms post-comando. */
    GSM_INIT_SEND_CMGF,       /* Inicia envio de AT+CMGF=1. */
    GSM_INIT_WAIT_CMGF_TX,    /* Espera confirmacion de TX. */
    GSM_INIT_WAIT_CMGF_DELAY, /* Pausa de 500 ms post-comando. */
    GSM_INIT_DONE             /* Modem listo para operar. */
} gsm_init_state_t;

static gsm_init_state_t gsm_state = GSM_INIT_WAIT_NETWORK;
static uint32_t         gsm_tick  = 0;

void gsm_driver_init(void) {
    gsm_state = GSM_INIT_WAIT_NETWORK;
    gsm_tick  = HAL_GetTick();
}

void gsm_driver_update(void) {
    switch (gsm_state) {

        case GSM_INIT_WAIT_NETWORK:
            if ((HAL_GetTick() - gsm_tick) >= 3000U)
                gsm_state = GSM_INIT_SEND_CLIP;
            break;

        case GSM_INIT_SEND_CLIP:
            flag_gsm_tx_done = 0;
            HAL_UART_Transmit_IT(&huart3, (uint8_t *)CMD_CLIP, sizeof(CMD_CLIP) - 1U);
            gsm_state = GSM_INIT_WAIT_CLIP_TX;
            break;

        case GSM_INIT_WAIT_CLIP_TX:
            if (flag_gsm_tx_done) {
                flag_gsm_tx_done = 0;
                gsm_tick  = HAL_GetTick();
                gsm_state = GSM_INIT_WAIT_CLIP_DELAY;
            }
            break;

        case GSM_INIT_WAIT_CLIP_DELAY:
            if ((HAL_GetTick() - gsm_tick) >= 500U)
                gsm_state = GSM_INIT_SEND_CMGF;
            break;

        case GSM_INIT_SEND_CMGF:
            flag_gsm_tx_done = 0;
            HAL_UART_Transmit_IT(&huart3, (uint8_t *)CMD_CMGF, sizeof(CMD_CMGF) - 1U);
            gsm_state = GSM_INIT_WAIT_CMGF_TX;
            break;

        case GSM_INIT_WAIT_CMGF_TX:
            if (flag_gsm_tx_done) {
                flag_gsm_tx_done = 0;
                gsm_tick  = HAL_GetTick();
                gsm_state = GSM_INIT_WAIT_CMGF_DELAY;
            }
            break;

        case GSM_INIT_WAIT_CMGF_DELAY:
            if ((HAL_GetTick() - gsm_tick) >= 500U)
                gsm_state = GSM_INIT_DONE;
            break;

        case GSM_INIT_DONE:
            break;
    }
}

bool gsm_driver_is_ready(void) {
    return (gsm_state == GSM_INIT_DONE);
}
