/*
 * sensor_gsm.c
 *
 * Deteccion y validacion de llamadas entrantes desde el modulo SIM800L.
 *
 * El callback procesa el stream UART byte a byte. Cuando el modem envia
 * una trama +CLIP (identificador de llamada), la almacena en el buffer
 * compartido y levanta un flag. La funcion update() cuelga la llamada con
 * ATH y verifica si el numero esta en alguna de las listas autorizadas.
 * El comando ATH se envia antes de la validacion para no bloquear la linea.
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
    if (flag_llamada_entrante == 0) return;
    flag_llamada_entrante = 0;

    /* Se cuelga la llamada en primer lugar para liberar el canal de voz. */
    HAL_UART_Transmit(&huart3, (uint8_t *)"ATH\r\nATH\r\n", 10, 100);

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
}
