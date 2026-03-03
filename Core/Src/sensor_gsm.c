/*
 * sensor_gsm.c
 *
 * Detecta llamadas entrantes desde el SIM800L y valida el numero.
 *
 * El callback acumula el stream UART y cuando llega una trama +CLIP
 * guarda el numero y levanta un flag. La update() cuelga la llamada
 * con ATH (no bloqueante) y chequea si el numero esta en alguna lista
 * autorizada antes de disparar el evento de panico.
 */

#include "sensor_gsm.h"
#include "app_events.h"
#include "main.h"
#include "usart.h"
#include <string.h>
#include <ctype.h>

/* Buffer privado de recepcion UART para el modem GSM (USART3). */
static uint8_t s_rx_byte_gsm;
static uint8_t s_rx_buf_gsm[100];
static uint8_t s_rx_idx_gsm = 0;

/* FSM interna para el proceso no bloqueante de colgar y validar la llamada. */
typedef enum {
    SGSM_IDLE,        /* Espera flag_llamada_entrante.                         */
    SGSM_SEND_ATH,    /* Intenta TX IT del comando ATH; reintenta si UART busy.*/
    SGSM_WAIT_ATH,    /* Aguarda 2 ms para que el modulo complete la TX.       */
    SGSM_VALIDATE     /* Extrae numero, valida y dispara ev_panico_llamada.    */
} sensor_gsm_fsm_t;

static sensor_gsm_fsm_t s_gsm_fsm  = SGSM_IDLE;
static uint32_t          s_gsm_tick = 0;

/* Buffer estatico requerido por HAL_UART_Transmit_IT (debe persistir durante TX). */
static const uint8_t ATH_CMD[] = "ATH\r\nATH\r\n";

static void trim_right(char *str) {
    int len = (int)strlen(str);
    while (len > 0 &&
           (str[len - 1] == ' ' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }
}

/* Busca el numero recibido en las tres listas de autorizacion de la RAM. */
static bool es_numero_autorizado(const char *num) {
    for (int i = 0; i < MAX_POLICIAS;  i++)
        if (lista_policias[i][0]  != '\0' && strstr(num, lista_policias[i])  != NULL) return true;
    for (int i = 0; i < MAX_CENTRALES; i++)
        if (lista_centrales[i][0] != '\0' && strstr(num, lista_centrales[i]) != NULL) return true;
    for (int i = 0; i < MAX_VECINOS;   i++)
        if (lista_vecinos[i][0]   != '\0' && strstr(num, lista_vecinos[i])   != NULL) return true;
    return false;
}

void sensor_gsm_init(void) {
    s_rx_idx_gsm = 0;
    memset(s_rx_buf_gsm, 0, sizeof(s_rx_buf_gsm));
    s_gsm_fsm  = SGSM_IDLE;
    s_gsm_tick = 0;
    HAL_UART_Receive_IT(&huart3, &s_rx_byte_gsm, 1);
}

/* Se ejecuta en ISR. Detecta el prompt '>', respuestas OK/ERROR y tramas +CLIP.
   No transmite nada por UART desde la ISR. */
void sensor_gsm_rx_callback(void) {
    if (s_rx_idx_gsm < 99U) {
        s_rx_buf_gsm[s_rx_idx_gsm++] = s_rx_byte_gsm;
    } else {
        s_rx_idx_gsm = 0;
        memset(s_rx_buf_gsm, 0, sizeof(s_rx_buf_gsm));
        s_rx_buf_gsm[s_rx_idx_gsm++] = s_rx_byte_gsm;
    }

    /* El prompt del modem llega como "> " (mayor y espacio). */
    if (s_rx_byte_gsm == ' ' && s_rx_idx_gsm >= 2 &&
        s_rx_buf_gsm[s_rx_idx_gsm - 2] == '>') {
        flag_gsm_prompt = 1;
        s_rx_idx_gsm    = 0;
        memset(s_rx_buf_gsm, 0, sizeof(s_rx_buf_gsm));
    } else if (s_rx_byte_gsm == '\n') {
        s_rx_buf_gsm[s_rx_idx_gsm] = '\0';
        char resp[100];
        strncpy(resp, (char *)s_rx_buf_gsm, 99);
        resp[99] = '\0';
        trim_right(resp);

        if (strstr(resp, "OK")    != NULL) flag_gsm_ok    = 1;
        if (strstr(resp, "ERROR") != NULL) flag_gsm_error = 1;

        /* Si la trama es +CLIP se guarda y se notifica a la FSM principal.
           La transmision de ATH queda pendiente para el contexto del loop. */
        if (strncmp(resp, "+CLIP: \"", 8) == 0) {
            strncpy(llamada_entrante_buffer, resp, 99);
            llamada_entrante_buffer[99] = '\0';
            flag_llamada_entrante = 1;
        }

        s_rx_idx_gsm = 0;
        memset(s_rx_buf_gsm, 0, sizeof(s_rx_buf_gsm));
    }

    HAL_UART_Receive_IT(&huart3, &s_rx_byte_gsm, 1);
}

void sensor_gsm_error_callback(void) {
    HAL_UART_Receive_IT(&huart3, &s_rx_byte_gsm, 1);
}

void sensor_gsm_update(void) {
    switch (s_gsm_fsm) {

        case SGSM_IDLE:
            if (flag_llamada_entrante == 0) return;
            flag_llamada_entrante = 0;
            s_gsm_fsm = SGSM_SEND_ATH;
            break;

        case SGSM_SEND_ATH:
            /* Si el periferico esta ocupado (sms_manager transmitiendo),
               se reintenta el proximo tick sin bloquear el ejecutor. */
            if (HAL_UART_Transmit_IT(&huart3, (uint8_t *)ATH_CMD,
                                     sizeof(ATH_CMD) - 1U) == HAL_OK) {
                s_gsm_tick = HAL_GetTick();
                s_gsm_fsm  = SGSM_WAIT_ATH;
            }
            break;

        case SGSM_WAIT_ATH:
            /* A 115200 baud, 10 bytes tardan ~0,87 ms. Con 2 ms hay margen. */
            if ((HAL_GetTick() - s_gsm_tick) >= 2U) {
                s_gsm_fsm = SGSM_VALIDATE;
            }
            break;

        case SGSM_VALIDATE:
            if (!sys_ocupado_sms) {
                char numero_entrante[20];
                memset(numero_entrante, 0, sizeof(numero_entrante));
                int i = 0;

                /* El numero viene entre comillas en la trama: +CLIP: "NUMERO",... */
                while (llamada_entrante_buffer[8 + i] != '\"' &&
                       llamada_entrante_buffer[8 + i] != '\0' && i < 19) {
                    numero_entrante[i] = llamada_entrante_buffer[8 + i];
                    i++;
                }
                numero_entrante[i] = '\0';

                if (es_numero_autorizado(numero_entrante)) {
                    memset(ev_numero_activador, 0, sizeof(ev_numero_activador));
                    strncpy(ev_numero_activador, numero_entrante, 19);
                    ev_panico_llamada = true;
                }
            }
            memset(llamada_entrante_buffer, 0, sizeof(llamada_entrante_buffer));
            s_gsm_fsm = SGSM_IDLE;
            break;
    }
}
