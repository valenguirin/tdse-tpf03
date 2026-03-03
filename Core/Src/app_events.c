/**
 *  app_events.c
 *  Definición única de todas las variables del bus de eventos inter-módulos.
 */
#include "app_events.h"

/* Eventos sensores → sistema */
volatile bool ev_panico_boton   = false;
volatile bool ev_panico_llamada = false;
volatile bool ev_silenciar      = false;
bool          ev_sys_night_mode = false;
bool          ev_sys_day_mode   = false;
bool          ev_sys_sms_done   = false;

/* Comandos sistema → actuadores */
bool cmd_siren_on       = false;
bool cmd_siren_off      = false;
bool cmd_strobe_on      = false;
bool cmd_strobe_off     = false;
bool cmd_led_rojo_on    = false;
bool cmd_led_rojo_off   = false;
bool cmd_ble_force_lock = false;

/* Estado compartido */
bool is_night        = false;
bool sys_ocupado_sms = false;
char ev_numero_activador[20] = "";
bool ble_session_open = false;

/* UI */
bool     ev_ui_error_blink = false;
uint32_t ui_tick_error     = 0;
uint32_t ui_tick_toggle    = 0;

/* Flags UART ISR */
volatile uint8_t flag_llamada_entrante = 0;
char             llamada_entrante_buffer[100] = "";
volatile uint8_t flag_gsm_prompt  = 0;
volatile uint8_t flag_gsm_ok      = 0;
volatile uint8_t flag_gsm_error   = 0;
volatile uint8_t flag_gsm_tx_done = 0;
volatile uint8_t flag_i2c_done    = 0;
volatile uint8_t flag_comando_ble = 0;
char             comando_ble_buffer[50] = "";

/* Bases de datos */
char lista_policias[MAX_POLICIAS][15]   = {"1133588475", "", ""};
char lista_centrales[MAX_CENTRALES][15] = {"1126332726", "", ""};
char lista_vecinos[MAX_VECINOS][15];

const char *LINK_MAPS = "https://www.google.com/maps/dir/?api=1&destination=-34.661042,-58.868015";
const char *LINK_WAZE = "https://www.waze.com/ul?ll=-34.661042%2C-58.868015&navigate=yes";

/* Cola SMS */
sms_task_t cola_sms[MAX_COLA_SMS];
uint8_t    sms_head      = 0;
uint8_t    sms_tail      = 0;
uint32_t   tick_delay_red = 0;
