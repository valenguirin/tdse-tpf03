/* Variables compartidas entre todos los modulos del sistema.
   Las definiciones estan en app_events.c */
#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

/* constantes */
#define MAX_POLICIAS  3
#define MAX_CENTRALES 3
#define MAX_VECINOS   200
#define MAX_COLA_SMS  20

/* estructura de la cola SMS */
typedef struct {
    char numero[15];
    char mensaje[100];
} sms_task_t;

/* eventos sensores → sistema */
extern volatile bool ev_panico_boton;    /* boton fisico confirmado por anti-rebote   */
extern volatile bool ev_panico_llamada;  /* llamada autorizada desde la red GSM       */
extern volatile bool ev_silenciar;       /* pedido de silencio por BLE                */
extern bool          ev_sys_night_mode;  /* LDR detecto noche sostenida               */
extern bool          ev_sys_day_mode;    /* LDR detecto dia sostenido                 */
extern bool          ev_sys_sms_done;    /* todas las tareas SMS completadas          */

/* comandos sistema → actuadores */
extern bool cmd_siren_on;
extern bool cmd_siren_off;
extern bool cmd_strobe_on;
extern bool cmd_strobe_off;
extern bool cmd_led_rojo_on;
extern bool cmd_led_rojo_off;
extern bool cmd_ble_force_lock;          /* fuerza cierre de sesion BLE               */

/* estado compartido */
extern bool is_night;                    /* modo noche activo                         */
extern bool sys_ocupado_sms;             /* sistema en emergencia activa              */
extern char ev_numero_activador[20];     /* numero que activo la alarma por llamada   */
extern bool ble_session_open;            /* sesion BLE autenticada                    */

/* UI - LED amarillo */
extern bool     ev_ui_error_blink;
extern uint32_t ui_tick_error;
extern uint32_t ui_tick_toggle;

/* flags de ISR - los setea el callback, los lee el modulo correspondiente */
extern volatile uint8_t flag_llamada_entrante;        /* +CLIP recibido desde GSM     */
extern char             llamada_entrante_buffer[100]; /* trama +CLIP completa         */
extern volatile uint8_t flag_gsm_prompt;              /* '>' recibido del modem       */
extern volatile uint8_t flag_gsm_ok;                  /* OK recibido del modem        */
extern volatile uint8_t flag_gsm_error;               /* ERROR recibido del modem     */
extern volatile uint8_t flag_gsm_tx_done;             /* TX UART3 completado          */
extern volatile uint8_t flag_i2c_done;                /* TX I2C1 Mem_Write_IT listo   */
extern volatile uint8_t flag_comando_ble;             /* comando BLE completo recibido*/
extern char             comando_ble_buffer[50];       /* texto del comando BLE        */

/* listas de numeros autorizados en RAM */
extern char        lista_policias[MAX_POLICIAS][15];
extern char        lista_centrales[MAX_CENTRALES][15];
extern char        lista_vecinos[MAX_VECINOS][15];
extern const char *LINK_MAPS;
extern const char *LINK_WAZE;

/* cola SMS */
extern sms_task_t cola_sms[MAX_COLA_SMS];
extern uint8_t    sms_head;
extern uint8_t    sms_tail;
extern uint32_t   tick_delay_red;

#endif /* APP_EVENTS_H */
