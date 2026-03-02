/*
 * gsm_driver.c
 *
 * Inicializacion no bloqueante del modem SIM800L.
 *
 * El modem necesita unos segundos para registrarse en la red celular antes
 * de recibir comandos AT. Esta FSM reemplaza los HAL_Delay de la version
 * anterior con temporizadores basados en HAL_GetTick(), de modo que el
 * sistema puede atender otros eventos mientras espera.
 *
 * Secuencia:
 *   1. Espera 3 s a que el modem se conecte a la red.
 *   2. Envia AT+CLIP=1 para habilitar el identificador de llamada.
 *   3. Espera 500 ms.
 *   4. Envia AT+CMGF=1 para modo texto en SMS.
 *   5. Espera 500 ms.
 *   6. Queda en estado DONE; gsm_driver_is_ready() devuelve true.
 *
 * Los envios usan HAL_UART_Transmit_IT. El flag flag_gsm_tx_done lo setea
 * sms_manager_tx_done_callback() desde HAL_UART_TxCpltCallback en main.c.
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
