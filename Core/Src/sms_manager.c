/*
 * sms_manager.c
 *
 * Motor de envio de SMS con cola circular y maquina de estados no bloqueante.
 *
 * Todas las transmisiones UART usan HAL_UART_Transmit_IT. El callback
 * sms_manager_tx_done_callback() se llama desde HAL_UART_TxCpltCallback
 * (USART3) en main.c y levanta flag_gsm_tx_done para desbloquear la FSM.
 *
 * El modem SIM800L requiere un protocolo de dos pasos para enviar un SMS:
 *   1. Se envia el comando AT+CMGS con el numero destino.
 *   2. El modem responde con '>'; recien entonces se envia el texto y 0x1A.
 * La FSM espera cada confirmacion antes de avanzar al paso siguiente.
 *
 * Mapa de estados:
 *   SMS_IDLE        : espera cola no vacia, GSM listo y retardo inicial de 4 s.
 *   SMS_SEND_CMD    : inicia TX IT del comando AT+CMGS.
 *   SMS_WAIT_TX_CMD : aguarda confirmacion de fin de TX del comando.
 *   SMS_WAIT_PROMPT : aguarda el prompt '>' o timeout de 3 s.
 *   SMS_SEND_MSG    : inicia TX IT del mensaje mas el byte 0x1A.
 *   SMS_WAIT_TX_MSG : aguarda confirmacion de fin de TX del mensaje.
 *   SMS_WAIT_OK     : aguarda OK, ERROR o timeout de 12 s del modem.
 */
#include "sms_manager.h"
#include "gsm_driver.h"
#include "app_events.h"
#include "main.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    SMS_IDLE,
    SMS_SEND_CMD,
    SMS_WAIT_TX_CMD,
    SMS_WAIT_PROMPT,
    SMS_SEND_MSG,
    SMS_WAIT_TX_MSG,
    SMS_WAIT_OK
} fsm_sms_state_t;

static fsm_sms_state_t estado_sms   = SMS_IDLE;
static uint32_t        tick_sms_fsm = 0;

/* Buffers estaticos: deben existir mientras dure la transmision IT. */
static uint8_t s_cmd_buf[40];
static uint8_t s_msg_buf[105];   /* 100 bytes de texto + byte 0x1A + margen */

void sms_manager_init(void) {
    estado_sms   = SMS_IDLE;
    tick_sms_fsm = 0;
}

/* Se llama desde HAL_UART_TxCpltCallback (USART3) en main.c. */
void sms_manager_tx_done_callback(void) {
    flag_gsm_tx_done = 1;
}

void sms_manager_encolar(const char *num, const char *msg) {
    if (num[0] == '\0') return;
    if (((sms_head + 1U) % MAX_COLA_SMS) == sms_tail) return;   /* Cola llena. */

    strncpy(cola_sms[sms_head].numero,  num, 14);
    cola_sms[sms_head].numero[14] = '\0';
    strncpy(cola_sms[sms_head].mensaje, msg, 99);
    cola_sms[sms_head].mensaje[99] = '\0';

    sms_head = (sms_head + 1U) % MAX_COLA_SMS;
}

/* Encola dos SMS por destinatario: ubicacion en Google Maps y en Waze. */
void sms_manager_generar_ruta(const char *tipo_activacion) {
    char msg_maps[100];
    snprintf(msg_maps, sizeof(msg_maps), "%s\n%s", tipo_activacion, LINK_MAPS);

    for (int i = 0; i < MAX_POLICIAS; i++) {
        sms_manager_encolar(lista_policias[i],  msg_maps);
        sms_manager_encolar(lista_policias[i],  LINK_WAZE);
    }
    for (int i = 0; i < MAX_CENTRALES; i++) {
        sms_manager_encolar(lista_centrales[i], msg_maps);
        sms_manager_encolar(lista_centrales[i], LINK_WAZE);
    }
}

void sms_manager_update(void) {
    switch (estado_sms) {

        case SMS_IDLE:
            /* Se requiere cola con datos, GSM inicializado y 4 s de retardo
               para que la red registre al modem antes del primer envio. */
            if (sms_tail != sms_head && gsm_driver_is_ready()) {
                if ((HAL_GetTick() - tick_delay_red) > 4000U) {
                    snprintf((char *)s_cmd_buf, sizeof(s_cmd_buf),
                             "AT+CMGS=\"%s\"\r\n", cola_sms[sms_tail].numero);
                    flag_gsm_prompt = 0;
                    flag_gsm_ok     = 0;
                    flag_gsm_error  = 0;
                    estado_sms      = SMS_SEND_CMD;
                }
            }
            break;

        case SMS_SEND_CMD:
            flag_gsm_tx_done = 0;
            HAL_UART_Transmit_IT(&huart3, s_cmd_buf, (uint16_t)strlen((char *)s_cmd_buf));
            tick_sms_fsm = HAL_GetTick();
            estado_sms   = SMS_WAIT_TX_CMD;
            break;

        case SMS_WAIT_TX_CMD:
            if (flag_gsm_tx_done) {
                flag_gsm_tx_done = 0;
                tick_sms_fsm     = HAL_GetTick();
                estado_sms       = SMS_WAIT_PROMPT;
            }
            break;

        case SMS_WAIT_PROMPT:
            /* El modem responde con '>' cuando esta listo para recibir el texto. */
            if (flag_gsm_prompt == 1) {
                flag_gsm_prompt = 0;

                /* El mensaje se copia al buffer estatico y se agrega 0x1A al final. */
                uint16_t len = (uint16_t)strlen(cola_sms[sms_tail].mensaje);
                if (len > 100U) len = 100U;
                memcpy(s_msg_buf, cola_sms[sms_tail].mensaje, len);
                s_msg_buf[len] = 0x1AU;

                estado_sms = SMS_SEND_MSG;
            } else if ((HAL_GetTick() - tick_sms_fsm) > 3000U) {
                /* Timeout: se descarta la tarea y se avanza al siguiente elemento. */
                sms_tail   = (sms_tail + 1U) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            }
            break;

        case SMS_SEND_MSG: {
            uint16_t len = (uint16_t)strlen(cola_sms[sms_tail].mensaje);
            if (len > 100U) len = 100U;
            flag_gsm_tx_done = 0;
            /* Se envia el texto mas el byte 0x1A que indica fin de mensaje al modem. */
            HAL_UART_Transmit_IT(&huart3, s_msg_buf, len + 1U);
            tick_sms_fsm = HAL_GetTick();
            estado_sms   = SMS_WAIT_TX_MSG;
            break;
        }

        case SMS_WAIT_TX_MSG:
            if (flag_gsm_tx_done) {
                flag_gsm_tx_done = 0;
                tick_sms_fsm     = HAL_GetTick();
                estado_sms       = SMS_WAIT_OK;
            }
            break;

        case SMS_WAIT_OK:
            /* OK o ERROR indican que el modem proceso el SMS; el timeout de 12 s es el seguro. */
            if (flag_gsm_ok == 1 || flag_gsm_error == 1) {
                sms_tail   = (sms_tail + 1U) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            } else if ((HAL_GetTick() - tick_sms_fsm) > 12000U) {
                sms_tail   = (sms_tail + 1U) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            }
            break;
    }

    /* Cuando la cola queda vacia se notifica al sistema central. */
    if (sys_ocupado_sms && estado_sms == SMS_IDLE && sms_tail == sms_head) {
        ev_sys_sms_done = true;
    }
}
