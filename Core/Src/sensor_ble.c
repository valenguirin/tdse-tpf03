/*
 * sensor_ble.c
 *
 * FSM de autenticacion Bluetooth y procesamiento de comandos de usuario.
 *
 * La recepcion UART es por interrupcion (un byte a la vez). El callback
 * acumula los bytes en un buffer interno y levanta un flag cuando llega '\n'.
 * La FSM de autenticacion tiene tres estados: bloqueado, esperando clave y
 * sesion abierta. Durante una emergencia activa solo se acepta el comando SILENCE.
 */

#include "sensor_ble.h"
#include "app_events.h"
#include "eeprom_driver.h"
#include "main.h"
#include "usart.h"
#include <string.h>
#include <ctype.h>

typedef enum {
    AUTH_LOCKED,        /* Estado inicial, el sistema ignora comandos hasta recibir ADMIN. */
    AUTH_WAITING_PASS,  /* Se recibio ADMIN, el sistema espera la clave FIUBA. */
    AUTH_OPEN           /* Sesion autenticada, el usuario tiene acceso a la base de datos. */
} auth_status_t;

static auth_status_t fsm_auth = AUTH_LOCKED;

/* Buffer privado para acumulacion de bytes UART del modulo BLE (USART1). */
static uint8_t s_rx_byte_ble;
static uint8_t s_rx_buf_ble[50];
static uint8_t s_rx_idx_ble = 0;

/* Elimina espacios y saltos de linea al final de la cadena. */
static void trim_right(char *str) {
    int len = (int)strlen(str);
    while (len > 0 &&
           (str[len - 1] == ' ' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }
}

/* Convierte toda la cadena a mayusculas para comparacion sin distincion de caso. */
static void to_upper(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

void sensor_ble_init(void) {
    fsm_auth     = AUTH_LOCKED;
    s_rx_idx_ble = 0;
    memset(s_rx_buf_ble, 0, sizeof(s_rx_buf_ble));
    HAL_UART_Receive_IT(&huart1, &s_rx_byte_ble, 1);
}

/* Se ejecuta en contexto de ISR. Solo acumula bytes y levanta el flag;
   no realiza ningun procesamiento de la cadena. */
void sensor_ble_rx_callback(void) {
    if (s_rx_idx_ble < 49U) {
        s_rx_buf_ble[s_rx_idx_ble++] = s_rx_byte_ble;
    } else {
        s_rx_idx_ble = 0;
        memset(s_rx_buf_ble, 0, sizeof(s_rx_buf_ble));
        s_rx_buf_ble[s_rx_idx_ble++] = s_rx_byte_ble;
    }

    if (s_rx_byte_ble == '\n') {
        s_rx_buf_ble[s_rx_idx_ble] = '\0';
        strncpy(comando_ble_buffer, (char *)s_rx_buf_ble, 49);
        comando_ble_buffer[49] = '\0';
        flag_comando_ble = 1;
        s_rx_idx_ble = 0;
        memset(s_rx_buf_ble, 0, sizeof(s_rx_buf_ble));
    }

    HAL_UART_Receive_IT(&huart1, &s_rx_byte_ble, 1);
}

/* Rearma la recepcion IT sin tocar el estado de la FSM ni los buffers. */
void sensor_ble_error_callback(void) {
    HAL_UART_Receive_IT(&huart1, &s_rx_byte_ble, 1);
}

void sensor_ble_update(void) {

    /* El sistema central puede forzar el cierre de sesion durante una alarma. */
    if (cmd_ble_force_lock) {
        fsm_auth           = AUTH_LOCKED;
        cmd_ble_force_lock = false;
    }

    /* La desconexion fisica del modulo BLE tambien cierra la sesion. */
    if (HAL_GPIO_ReadPin(BLE_STATE_Pin_GPIO_Port, BLE_STATE_Pin_Pin) == GPIO_PIN_RESET) {
        fsm_auth = AUTH_LOCKED;
    }

    /* Expone el estado de sesion al LED amarillo a traves del bus de eventos. */
    ble_session_open = (fsm_auth == AUTH_OPEN);

    if (flag_comando_ble == 0) return;
    flag_comando_ble = 0;

    trim_right(comando_ble_buffer);
    to_upper(comando_ble_buffer);

    /* Durante una emergencia activa solo se procesa el comando de silencio. */
    if (sys_ocupado_sms) {
        if (strcmp(comando_ble_buffer, "SILENCE") == 0) ev_silenciar = true;
        memset(comando_ble_buffer, 0, sizeof(comando_ble_buffer));
        return;
    }

    switch (fsm_auth) {

        case AUTH_LOCKED:
            if (strcmp(comando_ble_buffer, "ADMIN") == 0) {
                fsm_auth = AUTH_WAITING_PASS;
            } else {
                /* Clave incorrecta: activa el parpadeo de error en el LED amarillo. */
                ev_ui_error_blink = true;
                ui_tick_error     = HAL_GetTick();
                ui_tick_toggle    = HAL_GetTick();
            }
            break;

        case AUTH_WAITING_PASS:
            if (strcmp(comando_ble_buffer, "FIUBA") == 0) {
                fsm_auth = AUTH_OPEN;
            } else {
                fsm_auth          = AUTH_LOCKED;
                ev_ui_error_blink = true;
                ui_tick_error     = HAL_GetTick();
                ui_tick_toggle    = HAL_GetTick();
            }
            break;

        case AUTH_OPEN:
            if (strcmp(comando_ble_buffer, "OUT") == 0 ||
                strcmp(comando_ble_buffer, "SALIR") == 0) {
                fsm_auth = AUTH_LOCKED;
            } else if (strcmp(comando_ble_buffer, "SILENCE") == 0) {
                ev_silenciar = true;
            } else if (strncmp(comando_ble_buffer, "ADD ", 4) == 0) {
                eeprom_agregar_numero(comando_ble_buffer + 4);
            } else if (strncmp(comando_ble_buffer, "DEL ", 4) == 0) {
                eeprom_borrar_numero(comando_ble_buffer + 4);
            }
            break;
    }

    memset(comando_ble_buffer, 0, sizeof(comando_ble_buffer));
}
