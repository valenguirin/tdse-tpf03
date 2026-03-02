/**
 * @file    app_events.h
 * @brief   Bus de eventos inter-módulos del Sistema de Alarma Vecinal.
 *
 * Declara (extern) todas las variables compartidas entre módulos.
 * Cada módulo include este archivo para acceder al bus.
 * Las definiciones están en app_events.c.
 */
#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * CONSTANTES GLOBALES
 * ========================================================================= */
#define MAX_POLICIAS  3
#define MAX_CENTRALES 3
#define MAX_VECINOS   200
#define MAX_COLA_SMS  20

/* =========================================================================
 * ESTRUCTURA DE LA COLA SMS
 * ========================================================================= */
typedef struct {
    char numero[15];
    char mensaje[100];
} sms_task_t;

/* =========================================================================
 * EVENTOS: SENSORES → SISTEMA
 * ========================================================================= */
extern volatile bool ev_panico_boton;    /* Botón físico confirmado por anti-rebote.   */
extern volatile bool ev_panico_llamada;  /* Llamada autorizada desde red GSM.          */
extern volatile bool ev_silenciar;       /* Petición de silencio por BLE.              */
extern bool          ev_sys_night_mode;  /* LDR detectó noche sostenida.               */
extern bool          ev_sys_day_mode;    /* LDR detectó día sostenido.                 */
extern bool          ev_sys_sms_done;    /* Todas las tareas SMS completadas.          */

/* =========================================================================
 * COMANDOS: SISTEMA → ACTUADORES
 * ========================================================================= */
extern bool cmd_siren_on;
extern bool cmd_siren_off;
extern bool cmd_strobe_on;
extern bool cmd_strobe_off;
extern bool cmd_led_rojo_on;
extern bool cmd_led_rojo_off;
extern bool cmd_ble_force_lock;          /* Fuerza cierre de sesión BLE.              */

/* =========================================================================
 * ESTADO COMPARTIDO
 * ========================================================================= */
extern bool is_night;                    /* Modo noche activo.                         */
extern bool sys_ocupado_sms;             /* Sistema ocupado en emergencia.             */
extern char ev_numero_activador[20];     /* Número que activó la alarma por llamada.   */
extern bool ble_session_open;            /* Sesión BLE autenticada activa.             */

/* =========================================================================
 * EVENTOS UI (SENSOR_BLE → ACT_LED_AMARILLO)
 * ========================================================================= */
extern bool     ev_ui_error_blink;       /* Solicita parpadeo de error.                */
extern uint32_t ui_tick_error;           /* Marca de tiempo de inicio del error.       */
extern uint32_t ui_tick_toggle;          /* Marca de tiempo del último toggle.         */

/* =========================================================================
 * FLAGS UART ISR (SET EN CALLBACKS, LEÍDOS POR MÓDULOS)
 * ========================================================================= */
extern volatile uint8_t flag_llamada_entrante;   /* +CLIP recibido desde GSM.         */
extern char             llamada_entrante_buffer[100]; /* Trama +CLIP completa.         */
extern volatile uint8_t flag_gsm_prompt;         /* '>' recibido del módem.           */
extern volatile uint8_t flag_gsm_ok;             /* 'OK' recibido del módem.          */
extern volatile uint8_t flag_gsm_error;          /* 'ERROR' recibido del módem.       */
extern volatile uint8_t flag_gsm_tx_done;        /* TX UART3 completado.              */
extern volatile uint8_t flag_comando_ble;        /* Comando BLE completo recibido.    */
extern char             comando_ble_buffer[50];  /* Texto del comando BLE.            */

/* =========================================================================
 * BASES DE DATOS EN RAM
 * ========================================================================= */
extern char        lista_policias[MAX_POLICIAS][15];
extern char        lista_centrales[MAX_CENTRALES][15];
extern char        lista_vecinos[MAX_VECINOS][15];
extern const char *LINK_MAPS;
extern const char *LINK_WAZE;

/* =========================================================================
 * COLA SMS
 * ========================================================================= */
extern sms_task_t cola_sms[MAX_COLA_SMS];
extern uint8_t    sms_head;
extern uint8_t    sms_tail;
extern uint32_t   tick_delay_red;        /* Marca de tiempo de inicio de alarma.      */

#endif /* APP_EVENTS_H */
