/* USER CODE BEGIN Header */
/**
 * @file           : main.c
 * @brief          : Núcleo de control para el Sistema de Alarma Vecinal v5.
 * @details        : El programa implementa una arquitectura rigurosa basada en Máquinas de
 * Estado Finito (FSM) independientes, mapeadas directamente desde diagramas lógicos.
 * El diseño garantiza el desacoplamiento total entre la adquisición de datos (sensores),
 * la toma de decisiones (sistema) y la ejecución física (actuadores). La ejecución se
 * realiza mediante planificación cooperativa en un bucle principal.
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/*
+------------------------------------------------------------------------------+
| 1. DEFINICIÓN DE ESTADOS LÓGICOS PARA SENSORES Y SISTEMA                     |
+------------------------------------------------------------------------------+

*/

/* Máquina del Botón: El sistema ejecuta el rechazo de ruido eléctrico (debouncing). */
typedef enum {
    ST_BTN_PANIC_UP,      /* El contacto físico permanece inactivo. */
    ST_BTN_PANIC_FALLING, /* El microcontrolador detecta una transición inicial de caída. */
    ST_BTN_PANIC_DOWN,    /* El evento de presión se confirma tras el umbral de tiempo. */
    ST_BTN_PANIC_RISING   /* El sistema aguarda el retorno al nivel lógico alto. */
} fsm_btn_t;





/* Máquina LDR: El sistema retrasa la validación para prevenir falsas alertas por sombras. */
typedef enum {
    ST_LDR_DAY,          /* El ambiente registra intensidad lumínica alta. */
    ST_LDR_VERIFY_NIGHT, /* El sistema audita una interrupción de luz durante 2 segundos. */
    ST_LDR_NIGHT,        /* El ambiente registra oscuridad sostenida. */
    ST_LDR_VERIFY_DAY    /* El sistema audita el retorno de luz durante 2 segundos. */
} fsm_ldr_t;






/* Máquina GSM: El receptor aguarda notificaciones de la red celular pasivamente. */
typedef enum {
    ST_GSM_IDLE          /* El módem permanece en reposo a nivel de software. */
} fsm_gsm_t;





/* Máquina Bluetooth: El algoritmo impone seguridad de doble factor. */
typedef enum {
    ST_AUTH_LOCKED,       /* El acceso requiere el comando de inicialización. */
    ST_AUTH_WAITING_PASS, /* El sistema verifica la clave de seguridad de la institución. */
    ST_AUTH_OPEN          /* El acceso administrativo queda habilitado. */
} fsm_ble_t;





/* Sistema: El cerebro administra la respuesta global. */
typedef enum {
    ST_SYS_STANDBY,      /* El procesador central inspecciona pasivamente el bus de eventos. */
    ST_SYS_ALARM_ACTIVE  /* El sistema desencadena alarmas acústicas, lumínicas y mensajes. */
} fsm_sys_t;





/*
+------------------------------------------------------------------------------+
| 2. DEFINICIÓN DE ESTADOS LÓGICOS PARA ACTUADORES DE SALIDA                   |
+------------------------------------------------------------------------------+
| Los enumeradores dictan la operación del hardware de potencia .  |
+------------------------------------------------------------------------------+
*/

/* Máquina de Sirena:  señal de audio. */
typedef enum { ST_SIREN_OFF, ST_SIREN_ON } fsm_siren_t;




/* Máquina de Estrobo:  iluminación estroboscópica. */
typedef enum { ST_STROBE_LIGHT_OFF, ST_STROBE_LIGHT_ON } fsm_strobe_t;




/* Máquina del Indicador Rojo: Señala el estado operativo interno. */
typedef enum { ST_LED_ROJO_OFF, ST_LED_ROJO_ON } fsm_led_rojo_t;





/* Máquina del Indicador Amarillo: Señala estados y fallos de la sesión remota. */
typedef enum {
    ST_LED_AMARILLO_OFF,      /* El sistema carece de una sesión Bluetooth activa. */
    ST_LED_AMARILLO_ON,       /* El sistema sostiene una sesión administrativa abierta. */
    ST_LED_AMARILLO_BLINKING  /* El sistema acusa un error de autenticación. */
} fsm_led_amarillo_t;






/* Máquina SMS: El controlador asíncrono libera la CPU frente a la latencia de red. */
typedef enum {
    SMS_IDLE,        /* La estructura de datos carece de elementos a despachar. */
    SMS_WAIT_PROMPT, /* El puerto serie aguarda el carácter de confirmación del SIM800L. */
    SMS_WAIT_OK      /* El puerto serie aguarda la confirmación de envío exitoso. */
} fsm_sms_state_t;




/* Estructura para el empaquetado y encolado de notificaciones de emergencia. */
typedef struct {
    char numero[15];
    char mensaje[100];
} sms_task_t;
/* USER CODE END PTD */




/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EEPROM_ADDR 0xA0               /* Dirección I2C base del chip de memoria. */
#define MAGIC_BYTE  0xAB               /* Sello de integridad de la base de datos local. */
#define TAMAÑO_DIRECCION_EEPROM I2C_MEMADD_SIZE_16BIT
#define TIEMPO_SIRENA_MS 20000         /* Ventana temporal para la finalización automática. */
#define DELAY_BOTON_MS 50              /* Constante de retardo para el filtro del pulsador. */
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE BEGIN PV */
volatile uint32_t start_cycles = 0;
volatile uint32_t elapsed_cycles = 0;
volatile uint32_t max_cycles = 0;
/* USER CODE END PV */
/*
+------------------------------------------------------------------------------+
| 3. BUS DE EVENTOS (INTER-PROCESS COMMUNICATION)                              |
+------------------------------------------------------------------------------+

*/

/* Eventos desde el bloque de adquisición (Sensores) hacia el procesador central. */
bool ev_sys_panic_pressed = false; /* El botón reporta una presión física verificada. */
bool ev_sys_panic_call = false;    /* El receptor GSM reporta un emisor autorizado. */
bool ev_sys_silence = false;       /* El módulo remoto demanda la cancelación de alarmas. */
bool ev_sys_night_mode = false;    /* El LDR reporta el descenso crítico de luminosidad. */
bool ev_sys_day_mode = false;      /* El LDR reporta el incremento de luminosidad. */
bool ev_sys_sms_done = false;      /* El despachador reporta la finalización de sus tareas. */

/* Eventos desde el receptor Bluetooth hacia los indicadores visuales (UI). */
bool ev_ui_error_blink = false;    /* Demanda el inicio de la alerta visual por error. */
bool ev_ui_led_yellow_on = false;  /* Demanda la iluminación fija de sesión activa. */
bool ev_ui_led_yellow_off = false; /* Demanda el cese de iluminación de sesión. */

/* Comandos lógicos desde el procesador central hacia actuadores y periféricos. */
bool cmd_siren_on = false;         /* Impone el cierre del circuito acústico. */
bool cmd_siren_off = false;        /* Impone la apertura del circuito acústico. */
bool cmd_strobe_on = false;        /* Impone la activación del panel lumínico. */
bool cmd_strobe_off = false;       /* Impone la desactivación del panel lumínico. */
bool cmd_led_rojo_on = false;      /* Restablece la señal visual de disponibilidad. */
bool cmd_led_rojo_off = false;     /* Oculta la señal visual para indicar red ocupada. */
bool cmd_ble_force_lock = false;   /* Revoca los accesos remotos por protocolo de seguridad. */

/* Variables de contexto lógico interno del Sistema. */
bool is_night = false;             /* Preserva el registro del ciclo día/noche. */
char ev_numero_activador[20] = ""; /* Conserva el contacto responsable del disparo celular. */

/*
+------------------------------------------------------------------------------+
| 4. INSTANCIAS DE CONTEXTO Y REGISTROS DE TEMPORIZACIÓN                       |
+------------------------------------------------------------------------------+
| El sistema inicializa las máquinas en estados base y reserva los cronómetros |
| que sostienen la concurrencia no bloqueante.                                 |
+------------------------------------------------------------------------------+
*/
fsm_btn_t fsm_btn = ST_BTN_PANIC_UP;
fsm_ldr_t fsm_ldr = ST_LDR_DAY;
fsm_ble_t fsm_auth = ST_AUTH_LOCKED;
fsm_gsm_t fsm_gsm = ST_GSM_IDLE;
fsm_sys_t fsm_sys = ST_SYS_STANDBY;
fsm_siren_t fsm_sirena = ST_SIREN_OFF;
fsm_strobe_t fsm_estrobo = ST_STROBE_LIGHT_OFF;
fsm_led_rojo_t fsm_led_rojo = ST_LED_ROJO_ON;
fsm_led_amarillo_t fsm_led_amarillo = ST_LED_AMARILLO_OFF;

uint32_t tick_btn = 0;             /* Retardo de validación de pulsador. */
uint32_t tick_ldr = 0;             /* Retardo de transición día/noche. */
uint32_t tick_sirena = 0;          /* Reloj de finalización de alerta sonora. */
uint32_t tick_estrobo = 0;         /* Reloj de finalización de destellos. */
uint32_t tick_led_amarillo = 0;    /* Umbral temporal general para interfaz de usuario. */
uint32_t ui_tick_toggle = 0;       /* Lapso de conmutación para el parpadeo de error. */

/*
+------------------------------------------------------------------------------+
| 5. COMUNICACIONES Y DESPACHO DE TAREAS                                       |
+------------------------------------------------------------------------------+
*/
volatile uint8_t flag_comando_ble = 0;
char comando_ble_buffer[50] = "";
uint8_t rx_byte, rx_buffer[50], rx_index = 0;

volatile uint8_t flag_llamada_entrante = 0;
char llamada_entrante_buffer[100] = "";
uint8_t rx_byte_gsm, rx_buffer_gsm[100], rx_index_gsm = 0;
volatile uint8_t flag_gsm_prompt = 0, flag_gsm_ok = 0, flag_gsm_error = 0;

/* Administración de la cola circular. */
#define MAX_COLA_SMS 20
sms_task_t cola_sms[MAX_COLA_SMS];
uint8_t sms_head = 0, sms_tail = 0;
fsm_sms_state_t estado_sms = SMS_IDLE;
uint32_t tick_sms_fsm = 0, tick_delay_red = 0;
bool sys_ocupado_sms = false;

/* Configuración de bases de datos. */
const char* LINK_MAPS = "https://www.google.com/maps/dir/?api=1&destination=-34.661042,-58.868015";
const char* LINK_WAZE = "https://www.waze.com/ul?ll=-34.661042%2C-58.868015&navigate=yes";

#define MAX_POLICIAS 3
#define MAX_CENTRALES 3
#define MAX_VECINOS 200

char lista_policias[MAX_POLICIAS][15] = {"1133588475", "", ""};
char lista_centrales[MAX_CENTRALES][15] = {"1126332726", "", ""};
char lista_vecinos[MAX_VECINOS][15];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Prototipos de operaciones secundarias. */
void EEPROM_Init(void);
void EEPROM_Escribir_Slot(int indice);
void GSM_Init(void);
void String_Trim_Right(char *str);
void String_To_Upper(char *str);
bool Es_Numero_Autorizado(char* numero_entrante);
void Agregar_Numero(char* numero);
void Borrar_Numero(char* numero);
void Encolar_SMS(const char* num, const char* msg);
void Generar_Ruta_SMS(const char* tipo_activacion);

/* Prototipos principales de cada Máquina de Estado. */
void FSM_Sensor_LDR_Update(void);
void FSM_Sensor_PanicButton_Update(void);
void FSM_Sensor_BLE_Update(void);
void FSM_Sensor_GSM_Update(void);
void FSM_System_Update(void);
void FSM_Act_Siren_Update(void);
void FSM_Act_Strobe_Update(void);
void FSM_Act_LedRojo_Update(void);
void FSM_Act_LedAmarillo_Update(void);
void FSM_SMS_Update(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
+------------------------------------------------------------------------------+
| MÓDULO 1: SENSORES (MAPEO DIRECTO DESDE DIAGRAMAS DE ESTADO)                 |
+------------------------------------------------------------------------------+



 * @brief Máquina del LDR.
 * El algoritmo somete la lectura analógica a una ventana de verificación temporal.
 * El sistema asegura que los destellos momentáneos de luz no desactiven el estado nocturno.
 */
void FSM_Sensor_LDR_Update(void) {
    /* El procesador lee el pin físico y almacena el valor booleano de oscuridad. */
    bool is_dark = (HAL_GPIO_ReadPin(SENS_LDR_GPIO_Port, SENS_LDR_Pin) == GPIO_PIN_SET);

    /* El flujo de ejecución selecciona el camino basado en la memoria de estado actual. */
    switch(fsm_ldr) {
        case ST_LDR_DAY:
            /* Si el sensor reporta ausencia de luz, el sistema inicia la fase de auditoría. */
            if (is_dark) {
                fsm_ldr = ST_LDR_VERIFY_NIGHT;
                /* El microcontrolador registra la marca de tiempo de este suceso. */
                tick_ldr = HAL_GetTick();
            }
            break;

        case ST_LDR_VERIFY_NIGHT:
            /* Si la luz retorna antes de tiempo, el sistema aborta la auditoría y retorna al día. */
            if (!is_dark) {
                fsm_ldr = ST_LDR_DAY;
            }
            /* El algoritmo verifica si el lapso ininterrumpido supera los dos segundos. */
            else if ((HAL_GetTick() - tick_ldr) >= 2000) {
                fsm_ldr = ST_LDR_NIGHT;
                /* El módulo notifica al procesador central el inicio de la fase nocturna. */
                ev_sys_night_mode = true;
            }
            break;

        case ST_LDR_NIGHT:
            /* Si el sensor reporta presencia de luz, el sistema inicia la auditoría diurna. */
            if (!is_dark) {
                fsm_ldr = ST_LDR_VERIFY_DAY;
                tick_ldr = HAL_GetTick();
            }
            break;

        case ST_LDR_VERIFY_DAY:
            /* Si la oscuridad retorna prematuramente, el sistema aborta la auditoría. */
            if (is_dark) {
                fsm_ldr = ST_LDR_NIGHT;
            }
            /* El algoritmo exige estabilidad lumínica de dos segundos para cambiar el ciclo. */
            else if ((HAL_GetTick() - tick_ldr) >= 2000) {
                fsm_ldr = ST_LDR_DAY;
                /* El módulo notifica al procesador central el inicio de la fase diurna. */
                ev_sys_day_mode = true;
            }
            break;
    }
}

/**
 * @brief Máquina del Botón Físico.
 * El control aplica un límite restrictivo de 50 milisegundos para rechazar ruidos eléctricos.
 */
void FSM_Sensor_PanicButton_Update(void) {
    /* El microcontrolador captura el nivel eléctrico instantáneo del pulsador. */
    GPIO_PinState lectura_boton = HAL_GPIO_ReadPin(SENS_BOTON_PANICO_GPIO_Port, SENS_BOTON_PANICO_Pin);

    /* El sistema evalúa el nodo actual en la máquina anti-rebote. */
    switch (fsm_btn) {
        case ST_BTN_PANIC_UP:
            /* Ante un flanco de bajada (presión), la máquina registra el tiempo y avanza de fase. */
            if (lectura_boton == GPIO_PIN_RESET) {
                fsm_btn = ST_BTN_PANIC_FALLING;
                tick_btn = HAL_GetTick();
            }
            break;

        case ST_BTN_PANIC_FALLING:
            /* El algoritmo verifica la expiración del tiempo de guardia de 50 milisegundos. */
            if ((HAL_GetTick() - tick_btn) >= DELAY_BOTON_MS) {
                /* Si la señal permanece en nivel bajo, el software ratifica la acción humana. */
                if (lectura_boton == GPIO_PIN_RESET) {
                    fsm_btn = ST_BTN_PANIC_DOWN;
                    /* La máquina inyecta la alerta de pánico confirmada en el bus de eventos. */
                    ev_sys_panic_pressed = true;
                } else {
                    /* Si la señal asciende, el sistema descarta el suceso como ruido parasitario. */
                    fsm_btn = ST_BTN_PANIC_UP;
                }
            }
            break;

        case ST_BTN_PANIC_DOWN:
            /* El sistema aguarda la liberación física del actuador (retorno a nivel alto). */
            if (lectura_boton == GPIO_PIN_SET) {
                fsm_btn = ST_BTN_PANIC_RISING;
                tick_btn = HAL_GetTick();
            }
            break;

        case ST_BTN_PANIC_RISING:
             /* El algoritmo requiere 50 milisegundos de estabilidad en alto antes del reinicio. */
             if ((HAL_GetTick() - tick_btn) >= DELAY_BOTON_MS) {
                if (lectura_boton == GPIO_PIN_SET) {
                    fsm_btn = ST_BTN_PANIC_UP;
                } else {
                    /* Un nivel bajo intermitente devuelve la máquina al estado de compresión. */
                    fsm_btn = ST_BTN_PANIC_DOWN;
                }
             }
             break;
    }
}

/**
 * @brief Máquina Bluetooth (Autenticación).
 * El proceso maneja directrices forzadas de bloqueo y evalúa la sintaxis serial.
 */
void FSM_Sensor_BLE_Update(void) {
    /* El bloque atiende solicitudes de bloqueo inyectadas por el procesador central. */
    if (cmd_ble_force_lock) {
        fsm_auth = ST_AUTH_LOCKED;
        /* El sistema extingue la luz indicadora amarilla. */
        ev_ui_led_yellow_off = true;
        /* El comando se purga tras su ejecución. */
        cmd_ble_force_lock = false;
    }

    /* El hardware impone la caída de sesión ante una desconexión física del módulo. */
    if (HAL_GPIO_ReadPin(BLE_STATE_Pin_GPIO_Port, BLE_STATE_Pin_Pin) == GPIO_PIN_RESET) {
        if (fsm_auth != ST_AUTH_LOCKED) {
            fsm_auth = ST_AUTH_LOCKED;
            ev_ui_led_yellow_off = true;
        }
    }

    /* El analizador evalúa las instrucciones remitidas desde terminales móviles. */
    if (flag_comando_ble == 1) {
        /* El sistema consume la bandera de interrupción UART. */
        flag_comando_ble = 0;

        /* Las rutinas de limpieza remueven espacios vacíos y unifican el texto a mayúsculas. */
        String_Trim_Right(comando_ble_buffer);
        String_To_Upper(comando_ble_buffer);

        /* El núcleo rechaza alteraciones de base de datos durante ciclos de crisis activa. */
        if (sys_ocupado_sms) {
            /* El sistema admite de forma exclusiva la orden de silenciamiento en este periodo. */
            if (strcmp(comando_ble_buffer, "SILENCE") == 0) {
                ev_sys_silence = true;
            }
            /* El búfer de recepción recobra su estado vacío. */
            memset(comando_ble_buffer, 0, 50);
            return;
        }

        /* La lógica de acceso transita por tres barreras de seguridad. */
        switch(fsm_auth) {
            case ST_AUTH_LOCKED:
                /* La máquina avanza si el emisor introduce el prefijo 'ADMIN'. */
                if (strcmp(comando_ble_buffer, "ADMIN") == 0) {
                    fsm_auth = ST_AUTH_WAITING_PASS;
                } else {
                    /* Un comando erróneo dispara la señal visual de rechazo. */
                    ev_ui_error_blink = true;
                    ev_ui_led_yellow_off = true;
                }
                break;

            case ST_AUTH_WAITING_PASS:
                /* La máquina requiere la clave institucional 'FIUBA' para otorgar permisos. */
                if (strcmp(comando_ble_buffer, "FIUBA") == 0) {
                    fsm_auth = ST_AUTH_OPEN;
                    /* El proceso ilumina permanentemente el indicador de sesión. */
                    ev_ui_led_yellow_on = true;
                } else {
                    /* Un error de clave degrada el estado nuevamente a bloqueo total. */
                    fsm_auth = ST_AUTH_LOCKED;
                    ev_ui_error_blink = true;
                    ev_ui_led_yellow_off = true;
                }
                break;

            case ST_AUTH_OPEN:
                /* El usuario autorizado cuenta con un catálogo de operaciones válidas. */
                if (strcmp(comando_ble_buffer, "OUT") == 0 || strcmp(comando_ble_buffer, "SALIR") == 0) {
                    /* El comando finaliza la sesión por voluntad del administrador. */
                    fsm_auth = ST_AUTH_LOCKED;
                    ev_ui_led_yellow_off = true;
                }
                else if (strcmp(comando_ble_buffer, "SILENCE") == 0) {
                    ev_sys_silence = true;
                }
                /* Las subrutinas extraen el fragmento numérico y modifican la memoria permanente. */
                else if (strncmp(comando_ble_buffer, "ADD ", 4) == 0) {
                    Agregar_Numero(comando_ble_buffer + 4);
                }
                else if (strncmp(comando_ble_buffer, "DEL ", 4) == 0) {
                    Borrar_Numero(comando_ble_buffer + 4);
                }
                break;
        }
        /* El microcontrolador purga la memoria del comando ejecutado. */
        memset(comando_ble_buffer, 0, 50);
    }
}

/**
 * @brief Máquina de validación de tráfico GSM.
 * El bloque depura la trama cruda del SIM800L y coteja su validez en la base de datos.
 */
void FSM_Sensor_GSM_Update(void) {
    /* El código evalúa el comportamiento del receptor celular. */
    switch(fsm_gsm) {
        case ST_GSM_IDLE:
            /* El módulo abandona el reposo si la UART reporta una entrada tipo '+CLIP'. */
            if (flag_llamada_entrante == 1) {
                /* El software limpia la bandera de notificación de evento. */
                flag_llamada_entrante = 0;

                /* La validación numérica se descarta si el despachador SMS acapara el hardware. */
                if (!sys_ocupado_sms) {
                    char numero_entrante[20];
                    memset(numero_entrante, 0, 20);
                    int i = 0;

                    /* El bucle disecciona la cabecera serial para aislar los dígitos de origen. */
                    while (llamada_entrante_buffer[8 + i] != '\"' && llamada_entrante_buffer[8 + i] != '\0' && i < 19) {
                        numero_entrante[i] = llamada_entrante_buffer[8 + i];
                        i++;
                    }
                    /* El proceso asegura la integridad de la cadena mediante terminación nula. */
                    numero_entrante[i] = '\0';

                    /* La rutina compara la extracción con los registros de la matriz local. */
                    if (Es_Numero_Autorizado(numero_entrante)) {
                        /* El sistema transfiere el identificador seguro al contexto central. */
                        memset(ev_numero_activador, 0, 20);
                        strcpy(ev_numero_activador, numero_entrante);
                        /* La FSM eleva la condición de pánico hacia el cerebro. */
                        ev_sys_panic_call = true;
                    }
                }
                /* El procesador limpia el búfer UART preparándolo para la siguiente alerta. */
                memset(llamada_entrante_buffer, 0, 100);
            }
            break;
    }
}

/*
+------------------------------------------------------------------------------+
| MÓDULO 2: SISTEMA (CEREBRO ORQUESTADOR Y DECISORIO)                          |
+------------------------------------------------------------------------------+
*/

/**
 * @brief Máquina Central.
 * El sistema lee banderas periféricas en el bus de eventos y formula mandos de ejecución.
 */
void FSM_System_Update(void) {
    /* El proceso actualiza su entorno lógico interno ante cambios ambientales del LDR. */
    if (ev_sys_night_mode) { is_night = true; ev_sys_night_mode = false; }
    if (ev_sys_day_mode) { is_night = false; ev_sys_day_mode = false; }

    /* El procesador central direcciona el flujo según su modo operativo vigente. */
    switch(fsm_sys) {
        case ST_SYS_STANDBY:
            /* El sistema audita la ocurrencia de alteraciones confirmadas por los sensores. */
            if (ev_sys_panic_pressed || ev_sys_panic_call) {
                /* El cerebro decreta el cambio transversal hacia el modo de crisis. */
                fsm_sys = ST_SYS_ALARM_ACTIVE;

                /* La bandera de ocupación secuestra los recursos de red del módem celular. */
                sys_ocupado_sms = true;

                /* El reloj interno fija la estampa de tiempo para las demoras de seguridad. */
                tick_delay_red = HAL_GetTick();

                /* El núcleo difunde directrices booleanas hacia los componentes físicos. */
                cmd_siren_on = true;
                cmd_ble_force_lock = true;
                cmd_led_rojo_off = true;

                /* El algoritmo restringe el uso de destellos a las horas de oscuridad. */
                if (is_night) {
                    cmd_strobe_on = true;
                }

                /* El software compone las estructuras de texto e inicia el encolado masivo. */
                if (ev_sys_panic_pressed) {
                    Generar_Ruta_SMS("ALARMA BTN PANICO");
                }
                else if (ev_sys_panic_call) {
                    Generar_Ruta_SMS("ALARMA POR LLAMADA");
                    /* El módulo despacha un mensaje adicional a las entidades centrales. */
                    for(int i=0; i<MAX_CENTRALES; i++) {
                        char msg_num[50];
                        sprintf(msg_num, "Activado por la linea: %s", ev_numero_activador);
                        Encolar_SMS(lista_centrales[i], msg_num);
                    }
                }
            }
            break;

        case ST_SYS_ALARM_ACTIVE:
            /* El sistema monitorea la asimetría de los punteros del búfer SMS. */
            /* La igualdad de los índices certifica la conclusión de las transmisiones. */
            if (sms_tail == sms_head) {
                /* El cerebro devuelve la arquitectura al régimen de espera pasiva. */
                sys_ocupado_sms = false;
                fsm_sys = ST_SYS_STANDBY;
                cmd_led_rojo_on = true;
            }

            /* El proceso atiende demandas legítimas de interrupción acústica y óptica. */
            if (ev_sys_silence) {
                cmd_siren_off = true;
                cmd_strobe_off = true;
                /* El sistema consume el evento para impedir redundancia lógica. */
                ev_sys_silence = false;
            }
            break;
    }

    /* El ciclo finaliza borrando las alertas procesadas del bus IPC. */
    ev_sys_panic_pressed = false;
    ev_sys_panic_call = false;
}

/*
+------------------------------------------------------------------------------+
| MÓDULO 3: ACTUADORES FÍSICOS E INDICADORES VISUALES                          |
+------------------------------------------------------------------------------+
*/

/**
 * @brief Máquina Operativa de la Sirena.
 * El bloque gobierna el pin eléctrico y contabiliza el plazo de auto-desconexión.
 */
void FSM_Act_Siren_Update(void) {
    /* La rutina dictamina el comportamiento del relé según el estado presente. */
    switch(fsm_sirena) {
        case ST_SIREN_OFF:
            /* El microcontrolador anula la tensión en la base del transistor de control. */
            HAL_GPIO_WritePin(ACT_SIRENA_GPIO_Port, ACT_SIRENA_Pin, GPIO_PIN_RESET);

            /* Ante el mandato de ignición emitido por el cerebro, la FSM inicia la transición. */
            if (cmd_siren_on) {
                fsm_sirena = ST_SIREN_ON;
                /* La máquina captura el instante inicial del ciclo acústico. */
                tick_sirena = HAL_GetTick();
                /* La máquina elimina la bandera para finalizar la instrucción. */
                cmd_siren_on = false;
            }
            break;

        case ST_SIREN_ON:
            /* El procesador satura el puerto para mantener la corriente sobre la carga. */
            HAL_GPIO_WritePin(ACT_SIRENA_GPIO_Port, ACT_SIRENA_Pin, GPIO_PIN_SET);

            /* El flujo evalúa dos condiciones de salida: intervención manual o límite temporal. */
            if (cmd_siren_off || (HAL_GetTick() - tick_sirena >= TIEMPO_SIRENA_MS)) {
                /* Si se satisface la premisa, el sistema ordena el retiro al estado inactivo. */
                fsm_sirena = ST_SIREN_OFF;
                cmd_siren_off = false;
            }
            break;
    }
}

/**
 * @brief Máquina Operativa del Estrobo.
 * El módulo controla la luminaria intermitente de alerta.
 */
void FSM_Act_Strobe_Update(void) {
    /* La estructura condicional aísla las lógicas de encendido y apagado. */
    switch(fsm_estrobo) {
        case ST_STROBE_LIGHT_OFF:
            /* El puerto de salida fuerza un nivel bajo en el relé secundario. */
            HAL_GPIO_WritePin(ACT_ESTROBO_GPIO_Port, ACT_ESTROBO_Pin, GPIO_PIN_RESET);

            /* El sistema reacciona ante la petición exclusiva del cerebro. */
            if (cmd_strobe_on) {
                fsm_estrobo = ST_STROBE_LIGHT_ON;
                /* El registro temporal establece el umbral cero del ciclo de baliza. */
                tick_estrobo = HAL_GetTick();
                cmd_strobe_on = false;
            }
            break;

        case ST_STROBE_LIGHT_ON:
            /* El dispositivo físico recibe alimentación constante. */
            HAL_GPIO_WritePin(ACT_ESTROBO_GPIO_Port, ACT_ESTROBO_Pin, GPIO_PIN_SET);

            /* El temporizador de seguridad impone un cese tras 20 segundos de labor continua. */
            if (cmd_strobe_off || (HAL_GetTick() - tick_estrobo >= TIEMPO_SIRENA_MS)) {
                /* El flujo clausura la actividad lumínica. */
                fsm_estrobo = ST_STROBE_LIGHT_OFF;
                cmd_strobe_off = false;
            }
            break;
    }
}

/**
 * @brief Controlador del Indicador Principal (LED Rojo).
 * Refleja la capacidad del procesador central para asimilar nuevos eventos.
 */
void FSM_Act_LedRojo_Update(void) {
    /* El proceso alterna de forma directa los niveles de voltaje según su fase nominal. */
    switch(fsm_led_rojo) {
        case ST_LED_ROJO_OFF:
            /* El sistema apaga el indicador para comunicar sobrecarga transitoria de red. */
            HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);
            /* El microcontrolador reconoce la señal de reapertura. */
            if (cmd_led_rojo_on) {
                fsm_led_rojo = ST_LED_ROJO_ON;
                cmd_led_rojo_on = false;
            }
            break;

        case ST_LED_ROJO_ON:
            /* El sistema confiere energía constante al diodo para atestiguar su disponibilidad. */
            HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);
            /* El núcleo admite la orden restrictiva durante el inicio de una alarma. */
            if (cmd_led_rojo_off) {
                fsm_led_rojo = ST_LED_ROJO_OFF;
                cmd_led_rojo_off = false;
            }
            break;
    }
}

/**
 * @brief Controlador del Indicador de Red Local (LED Amarillo).
 * El sistema interpreta eventos de la máquina BLE para emitir señales visuales complejas.
 */
void FSM_Act_LedAmarillo_Update(void) {
    /* La rutina inspecciona el comportamiento de su FSM asociada. */
    switch(fsm_led_amarillo) {
        case ST_LED_AMARILLO_OFF:
            /* El hardware mantiene el pin inactivo por ausencia de sesión. */
            HAL_GPIO_WritePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin, GPIO_PIN_RESET);

            /* El algoritmo concede jerarquía absoluta a las notificaciones de error. */
            if (ev_ui_error_blink) {
                fsm_led_amarillo = ST_LED_AMARILLO_BLINKING;
                /* El proceso resetea simultáneamente los temporizadores base y de oscilación. */
                tick_led_amarillo = HAL_GetTick();
                ui_tick_toggle = HAL_GetTick();
                ev_ui_error_blink = false;
            }
            /* En ausencia de errores, la máquina admite la petición de encendido estable. */
            else if (ev_ui_led_yellow_on) {
                fsm_led_amarillo = ST_LED_AMARILLO_ON;
                ev_ui_led_yellow_on = false;
            }
            break;

        case ST_LED_AMARILLO_ON:
            /* El componente físico emite luz para validar la vinculación del dispositivo móvil. */
            HAL_GPIO_WritePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin, GPIO_PIN_SET);

            /* La introducción de sintaxis errónea provoca el abandono inmediato de la fase. */
            if (ev_ui_error_blink) {
                fsm_led_amarillo = ST_LED_AMARILLO_BLINKING;
                tick_led_amarillo = HAL_GetTick();
                ui_tick_toggle = HAL_GetTick();
                ev_ui_error_blink = false;
            }
            /* El sistema extingue el led tras el cierre lícito de sesión. */
            else if (ev_ui_led_yellow_off) {
                fsm_led_amarillo = ST_LED_AMARILLO_OFF;
                ev_ui_led_yellow_off = false;
            }
            break;

        case ST_LED_AMARILLO_BLINKING:
            /* La subrutina invierte periódicamente el estado lógico cada cien milisegundos. */
            if (HAL_GetTick() - ui_tick_toggle >= 100) {
                HAL_GPIO_TogglePin(LED_SYS_ARMED_GPIO_Port, LED_SYS_ARMED_Pin);
                ui_tick_toggle = HAL_GetTick();
            }
            /* El reloj interno restaura la condición pasiva una vez consumido un segundo total. */
            if (HAL_GetTick() - tick_led_amarillo >= 1000) {
                fsm_led_amarillo = ST_LED_AMARILLO_OFF;
            }
            break;
    }
}

/*
+------------------------------------------------------------------------------+
| MÓDULO 4: ADMINISTRADOR DE FLUJO SMS (MOTOR ASÍNCRONO)                       |
+------------------------------------------------------------------------------+
*/

/**
 * @brief Máquina Despachadora de Tareas (Motor GSM).
 * El bucle evalúa comandos AT fraccionados. Esta fragmentación asegura que el
 * procesador destine ciclos a los sensores sin sucumbir ante retardos de transmisión.
 */
void FSM_SMS_Update(void) {
    /* El proceso selecciona el flujo de control apropiado en el protocolo asíncrono. */
    switch (estado_sms) {
        case SMS_IDLE:
            /* El despachador revisa la asimetría en los punteros de la memoria circular. */
            /* Una diferencia señala la presencia de peticiones en la cola. */
            if (sms_tail != sms_head) {
                /* El algoritmo exige un periodo de gracia previo de cuatro segundos
                 * para afirmar la estabilidad de red del dispositivo SIM. */
                if ((HAL_GetTick() - tick_delay_red) > 4000) {
                    char cmd[40];

                    /* El procesador adjunta el contacto de destino al comando estándar AT. */
                    sprintf(cmd, "AT+CMGS=\"%s\"\r\n", cola_sms[sms_tail].numero);

                    /* El sistema suprime residuos previos en las banderas de interrupción UART. */
                    flag_gsm_prompt = 0;
                    flag_gsm_ok = 0;
                    flag_gsm_error = 0;

                    /* El microcontrolador expide el arreglo de caracteres al bus físico. */
                    HAL_UART_Transmit(&huart3, (uint8_t*)cmd, strlen(cmd), 100);

                    /* El módulo reserva el tiempo local e incrementa la fase de negociación. */
                    tick_sms_fsm = HAL_GetTick();
                    estado_sms = SMS_WAIT_PROMPT;
                }
            }
            break;

        case SMS_WAIT_PROMPT:
            /* El código concede libertades a la CPU y solo actúa cuando el transceptor
             * acusa recibo del comando de preparación con un carácter 'mayor que'. */
            if (flag_gsm_prompt == 1) {
                /* El software reconoce el aviso y apaga el indicador de recepción. */
                flag_gsm_prompt = 0;

                /* El hardware transmite la carga útil del mensaje extraída del arreglo. */
                HAL_UART_Transmit(&huart3, (uint8_t*)cola_sms[sms_tail].mensaje, strlen(cola_sms[sms_tail].mensaje), 500);

                /* El procedimiento sella la transmisión adjuntando el octeto finalizador de trama. */
                uint8_t ctrl_z = 0x1A;
                HAL_UART_Transmit(&huart3, &ctrl_z, 1, 100);

                /* El sistema actualiza el registro temporal para supeditar la respuesta definitiva. */
                tick_sms_fsm = HAL_GetTick();
                estado_sms = SMS_WAIT_OK;
            }
            /* El bloque descarta elementos que superan los tres segundos de silencio terminal. */
            else if ((HAL_GetTick() - tick_sms_fsm) > 3000) {
                /* El algoritmo elude la saturación incrementando el puntero ciego y aborta. */
                sms_tail = (sms_tail + 1) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            }
            break;

        case SMS_WAIT_OK:
            /* El proceso transfiere el control periódicamente hasta divisar veredictos
             * de cierre por parte del proveedor de red celular. */
            if (flag_gsm_ok == 1 || flag_gsm_error == 1) {
                /* El módulo desecha un índice resuelto y retorna al eslabón neutral. */
                sms_tail = (sms_tail + 1) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            }
            /* El sistema castiga latencias perjudiciales, cancelando tareas de red de doce segundos. */
            else if ((HAL_GetTick() - tick_sms_fsm) > 12000) {
                /* El puntero suprime la entrada trabada y retoma su estado de escucha. */
                sms_tail = (sms_tail + 1) % MAX_COLA_SMS;
                estado_sms = SMS_IDLE;
            }
            break;
    }
}
/* USER CODE END 0


+------------------------------------------------------------------------------+
|RUTINAS DE PERSISTENCIA Y FUNCIONES TRANSVERSALES                  |
+------------------------------------------------------------------------------+
| Este segmento agrupa las utilidades de almacenamiento físico (EEPROM),       |
| configuración de módulos, limpieza de texto y gestión de estructuras de datos|
| en la memoria RAM.                                                           |
+------------------------------------------------------------------------------+



 * @brief  Guarda o elimina un registro telefónico en la memoria EEPROM física.
 * @param  indice Posición en la matriz de la RAM (0 a 199).
 */
void EEPROM_Escribir_Slot(int indice) {
    /* El microcontrolador sondea el bus I2C para verificar la disponibilidad del chip.
       Si el chip no responde tras 50 milisegundos, la función aborta su ejecución. */
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDR, 2, 50) != HAL_OK) return;

    /* El algoritmo calcula la dirección física exacta en la EEPROM.
       Reserva los primeros 16 bytes para configuraciones y asigna bloques de 16 bytes
       para cada uno de los 200 usuarios posibles. */
    uint16_t addr = 16 + (indice * 16);

    /* El sistema detecta una solicitud de borrado si el primer carácter del registro es nulo. */
    if (lista_vecinos[indice][0] == '\0') {
        uint8_t vacio = '\0';
        /* El hardware sobrescribe únicamente el primer byte con un valor nulo para invalidar
           el registro completo. Esto optimiza el tiempo y evita desgastar la memoria. */
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, addr, TAMAÑO_DIRECCION_EEPROM, &vacio, 1, 50);
        /* El protocolo I2C exige un reposo de 5 milisegundos tras una operación de escritura. */
        HAL_Delay(5);
        return;
    }

    /* Si el registro contiene un número válido, el bucle transfiere los datos a la memoria física. */
    for (int j = 0; j < 15; j++) {
        /* El microcontrolador escribe byte por byte en las direcciones consecutivas del chip. */
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, addr + j, TAMAÑO_DIRECCION_EEPROM, (uint8_t*)&lista_vecinos[indice][j], 1, 50);
        HAL_Delay(5); /* Pausa obligatoria para la consolidación electrónica del dato en la EEPROM. */

        /* El proceso interrumpe las iteraciones de forma prematura al detectar el fin de la cadena,
           ahorrando ciclos de reloj valiosos. */
        if (lista_vecinos[indice][j] == '\0') break;
    }
}

/**
 * @brief  Inicializa el entorno de almacenamiento y recupera los datos guardados tras un reinicio.
 */
void EEPROM_Init(void) {
    /* El sistema audita la presencia física del chip de memoria en la placa. */
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDR, 3, 100) != HAL_OK) return;

    uint8_t check_byte = 0;
    /* El procesador extrae el primer byte de la EEPROM (Dirección 0). Este byte actúa como sello de integridad. */
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0, TAMAÑO_DIRECCION_EEPROM, &check_byte, 1, 100);

    /* El condicional evalúa si el sello extraído coincide con la firma predefinida (MAGIC_BYTE). */
    if (check_byte == MAGIC_BYTE) {
        /* Una firma válida confirma la existencia de datos previos. El bucle procede a extraer
           los 200 registros telefónicos desde la EEPROM hacia la matriz de acceso rápido en RAM. */
        for (int i = 0; i < MAX_VECINOS; i++) {
            uint16_t mem_addr = 16 + (i * 16);
            /* La rutina de lectura transfiere bloques completos de 15 bytes por cada vecino. */
            HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, mem_addr, TAMAÑO_DIRECCION_EEPROM, (uint8_t*)lista_vecinos[i], 15, 10);
            /* El software fuerza un carácter nulo en la última posición para asegurar el formato de cadena. */
            lista_vecinos[i][14] = '\0';
        }
    } else {
        /* La ausencia de la firma indica una EEPROM virgen o corrupta. El sistema inicia un formateo de fábrica. */
        check_byte = MAGIC_BYTE;
        /* El microcontrolador inscribe la firma válida en el primer sector para futuros reinicios. */
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, 0, TAMAÑO_DIRECCION_EEPROM, &check_byte, 1, 100);
        HAL_Delay(10);

        /* El software limpia la matriz de la memoria RAM con caracteres nulos. */
        for (int i = 0; i < MAX_VECINOS; i++) lista_vecinos[i][0] = '\0';

        uint8_t vacio = '\0';
        /* El proceso transfiere esta limpieza a la memoria EEPROM física, sector por sector. */
        for (int i = 0; i < MAX_VECINOS; i++) {
            uint16_t mem_addr = 16 + (i * 16);
            HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, mem_addr, TAMAÑO_DIRECCION_EEPROM, &vacio, 1, 50);
            HAL_Delay(5);
        }
        /* Finalmente, el algoritmo inscribe un número maestro predeterminado en el primer slot (índice 0)
           para garantizar el acceso inicial al sistema, y lo guarda permanentemente. */
        strcpy(lista_vecinos[0], "1133588475");
        EEPROM_Escribir_Slot(0);
    }
}

/**
 * @brief  Configura el transceptor celular SIM800L.
 */
void GSM_Init(void) {
    /* El sistema detiene su ejecución durante 3 segundos para permitir que el módem
       negocie su conexión con las antenas de la red celular. */
    HAL_Delay(3000);

    /* El comando AT+CLIP=1 activa la presentación del identificador de llamadas entrantes. */
    char cmd_clip[] = "AT+CLIP=1\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd_clip, strlen(cmd_clip), 500);
    HAL_Delay(500); /* Pausa para procesar la respuesta del módem. */

    /* El comando AT+CMGF=1 configura el módulo para enviar y recibir mensajes SMS en formato de texto plano. */
    char cmd_cmgf[] = "AT+CMGF=1\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd_cmgf, strlen(cmd_cmgf), 500);
    HAL_Delay(500); /* Pausa para procesar la respuesta del módem. */
}

/**
 * @brief  Suprime caracteres invisibles al final de una cadena de texto.
 */
void String_Trim_Right(char *str) {
    /* La rutina determina la cantidad total de caracteres útiles. */
    int len = strlen(str);
    /* El bucle analiza la cadena desde la última posición hacia atrás. Si detecta espacios
       en blanco o retornos de carro, los reemplaza por terminadores nulos para limpiar el texto. */
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}

/**
 * @brief  Convierte todos los caracteres de una cadena a letras mayúsculas.
 */
void String_To_Upper(char *str) {
    /* El iterador avanza por cada elemento del arreglo. La función toupper() estandariza
       el formato del texto para evitar fallos de autenticación por sensibilidad a mayúsculas. */
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

/**
 * @brief  Audita la identidad de un número telefónico contra las listas de seguridad.
 * @return Retorna 'true' si el número existe en los registros, de lo contrario 'false'.
 */
bool Es_Numero_Autorizado(char* numero_entrante) {
    /* El algoritmo rastrea el número entrante dentro de la matriz de autoridades policiales. */
    for(int i = 0; i < MAX_POLICIAS; i++) {
        if(lista_policias[i][0] != '\0' && strstr(numero_entrante, lista_policias[i]) != NULL) return true;
    }
    /* El algoritmo rastrea el número dentro de la matriz de centrales de monitoreo. */
    for(int i = 0; i < MAX_CENTRALES; i++) {
        if(lista_centrales[i][0] != '\0' && strstr(numero_entrante, lista_centrales[i]) != NULL) return true;
    }
    /* Finalmente, rastrea la identidad en la extensa lista de vecinos locales en la RAM. */
    for(int i = 0; i < MAX_VECINOS; i++) {
        if(lista_vecinos[i][0] != '\0' && strstr(numero_entrante, lista_vecinos[i]) != NULL) return true;
    }
    /* Si las tres búsquedas fracasan, el sistema rechaza el contacto. */
    return false;
}

/**
 * @brief  Registra un nuevo usuario en la base de datos local.
 */
void Agregar_Numero(char* numero) {
    /* El bucle recorre la memoria en RAM en busca del primer espacio disponible (vacío). */
    for(int i = 0; i < MAX_VECINOS; i++) {
        if(lista_vecinos[i][0] == '\0') {
            /* Al hallar un hueco, el programa copia el nuevo número en la RAM. */
            strcpy(lista_vecinos[i], numero);
            /* Inmediatamente, la rutina invoca la escritura física para preservar el dato. */
            EEPROM_Escribir_Slot(i);
            return;
        }
    }
}

/**
 * @brief  Elimina un usuario específico de la base de datos.
 */
void Borrar_Numero(char* numero) {
    /* El bucle escanea la memoria RAM hasta encontrar una coincidencia exacta con el objetivo. */
    for(int i = 0; i < MAX_VECINOS; i++) {
        if(strcmp(lista_vecinos[i], numero) == 0) {
            /* El proceso formatea el bloque de RAM con ceros absolutos para borrar rastros. */
            memset(lista_vecinos[i], 0, 15);
            /* La función ordena a la memoria persistente replicar este borrado físicamente. */
            EEPROM_Escribir_Slot(i);
            return;
        }
    }
}

/**
 * @brief  Inyecta un paquete de datos en el sistema de mensajería asíncrona.
 */
void Encolar_SMS(const char* num, const char* msg) {
    /* El filtro descarta la operación si el destinatario no posee un identificador válido. */
    if (num[0] == '\0') return;

    /* El sistema calcula la próxima posición en el búfer circular. Si el índice de
       entrada colisiona con el de salida, la cola está saturada y la función aborta. */
    if (((sms_head + 1) % MAX_COLA_SMS) == sms_tail) return;

    /* El programa traslada el número y el texto hacia la estructura de datos seleccionada.
       Los límites numéricos protegen al sistema contra desbordamientos de búfer. */
    strncpy(cola_sms[sms_head].numero, num, 14);
    strncpy(cola_sms[sms_head].mensaje, msg, 99);

    /* El índice de entrada avanza una posición mediante aritmética modular. */
    sms_head = (sms_head + 1) % MAX_COLA_SMS;
}

/**
 * @brief  Compila los textos de emergencia y los remite a los entes de seguridad.
 */
void Generar_Ruta_SMS(const char* tipo_activacion) {
    char msg_maps[100];
    /* La rutina concatena la causa del pánico (Botón o Llamada) con un hipervínculo
       de coordenadas geográficas hacia la plataforma Google Maps. */
    sprintf(msg_maps, "%s\n%s", tipo_activacion, LINK_MAPS);

    /* El ciclo inyecta dos tareas por cada oficial en la cola de mensajes:
       un enlace para Google Maps y un enlace directo para Waze. */
    for(int i=0; i<MAX_POLICIAS; i++) {
        Encolar_SMS(lista_policias[i], msg_maps);
        Encolar_SMS(lista_policias[i], LINK_WAZE);
    }
    /* El ciclo replica el proceso de notificación para las centrales de monitoreo. */
    for(int i=0; i<MAX_CENTRALES; i++) {
        Encolar_SMS(lista_centrales[i], msg_maps);
        Encolar_SMS(lista_centrales[i], LINK_WAZE);
    }
}

/* USER CODE END 0 */

/**
 * @brief Orquestador Maestro del Microcontrolador.
 * Contiene las rutinas de inicialización de hardware y el planificador continuo de procesos.
 */
int main(void)
{
  /* Inicialización de la Capa de Abstracción de Hardware (HAL). */
  HAL_Init();

  /* Configuración del árbol de frecuencias para operar el núcleo a 72MHz. */
  SystemClock_Config();

  /* Configuración de los periféricos de propósito general (Pines). */
  MX_GPIO_Init();
  /* Configuración del protocolo I2C para comunicación con la memoria. */
  MX_I2C1_Init();
  /* Configuración de los puertos seriales para comunicación Bluetooth, depuración y red GSM. */
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();

  /* USER CODE BEGIN 2 */
  EEPROM_Init();
  /* El procesador configura el módulo celular. */
  GSM_Init();

   /* VARIABLES PARA MEDIR WCET POR SOFTWARE */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* USER CODE END 2 */
  /* El sistema invoca la carga y validación de la base de datos en RAM. */

  /* El núcleo habilita el servicio de interrupciones para capturar caracteres
     asíncronos desde el Bluetooth y el módem sin bloquear procesos. */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  HAL_UART_Receive_IT(&huart3, &rx_byte_gsm, 1);
  /* USER CODE END 2 */

  /**
   * @section BUCLE_INFINITO (SUPER-LOOP COOPERATIVO)
   * La organización secuencial ejecuta los bloques lógicos de forma continua.
   * El diseño segmentado en FSM garantiza la evaluación constante del botón de
   * pánico, priorizando el escaneo de amenazas frente a latencias de red.
   */
  while (1)
  {
	  start_cycles = DWT->CYCCNT;
      /* Bloque de Adquisición: El sistema captura y valida señales físicas. */
      FSM_Sensor_LDR_Update();
      FSM_Sensor_PanicButton_Update();
      FSM_Sensor_BLE_Update();
      FSM_Sensor_GSM_Update();

      /* Bloque Lógico: El cerebro toma decisiones basadas en los eventos detectados. */
      FSM_System_Update();

      /* Bloque de Potencia: Los actuadores encienden y apagan el hardware periférico. */
      FSM_Act_Siren_Update();
      FSM_Act_Strobe_Update();
      FSM_Act_LedRojo_Update();
      FSM_Act_LedAmarillo_Update();

      /* Bloque de Transmisión: El motor UART avanza pasos en la negociación con la red. */
      FSM_SMS_Update();

      /* Retardo prudencial: Impone un límite temporal para la disipación térmica del procesador. */
      /* Detiene el cronómetro y calcula la diferencia */
            elapsed_cycles = DWT->CYCCNT - start_cycles;

            /* Si el ciclo actual tardó más que el máximo histórico, lo actualiza */
            if (elapsed_cycles > max_cycles) {
                max_cycles = elapsed_cycles;
            }

            HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  }
}
/**
 * @brief Parametrización del arreglo de osciladores de silicio.
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

/* USER CODE BEGIN 4 */

/**
 * @brief Adquisición asíncrona de datos desde puertos físicos (ISRs).
 * El procesador almacena flujos de bits entrantes sin degradar el bucle principal.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

  /* Monitor de módulo Bluetooth. */
  if (huart->Instance == USART1) {
    if (rx_index < 49) rx_buffer[rx_index++] = rx_byte;
    else {
        rx_index = 0;
        memset(rx_buffer, 0, 50);
        rx_buffer[rx_index++] = rx_byte;
    }

    if (rx_byte == '\n') {
        rx_buffer[rx_index] = '\0';
        strcpy(comando_ble_buffer, (char*)rx_buffer);
        flag_comando_ble = 1;
        rx_index = 0;
        memset(rx_buffer, 0, 50);
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }

  /* Monitor de transceptor GSM. */
  if (huart->Instance == USART3) {
    if (rx_index_gsm < 99) rx_buffer_gsm[rx_index_gsm++] = rx_byte_gsm;
    else {
        rx_index_gsm = 0;
        memset(rx_buffer_gsm, 0, 100);
        rx_buffer_gsm[rx_index_gsm++] = rx_byte_gsm;
    }

    if (rx_byte_gsm == ' ' && rx_index_gsm >= 2 && rx_buffer_gsm[rx_index_gsm-2] == '>') {
        flag_gsm_prompt = 1;
        rx_index_gsm = 0;
        memset(rx_buffer_gsm, 0, 100);
    }
    else if (rx_byte_gsm == '\n') {
        rx_buffer_gsm[rx_index_gsm] = '\0';
        char respuesta_gsm[100];
        strcpy(respuesta_gsm, (char*)rx_buffer_gsm);
        String_Trim_Right(respuesta_gsm);

        if (strstr(respuesta_gsm, "OK") != NULL) flag_gsm_ok = 1;
        if (strstr(respuesta_gsm, "ERROR") != NULL) flag_gsm_error = 1;

        if (strncmp(respuesta_gsm, "+CLIP: \"", 8) == 0) {
            /* El equipo fuerza un corte inmediato de línea para repeler ocupaciones indebidas de red. */
            HAL_UART_Transmit(&huart3, (uint8_t*)"ATH\r\nATH\r\n", 10, 100);
            strcpy(llamada_entrante_buffer, respuesta_gsm);
            flag_llamada_entrante = 1;
        }

        rx_index_gsm = 0;
        memset(rx_buffer_gsm, 0, 100);
    }
    HAL_UART_Receive_IT(&huart3, &rx_byte_gsm, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) HAL_UART_Receive_IT(&huart3, &rx_byte_gsm, 1);
    if (huart->Instance == USART1) HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) { }
/* USER CODE END 4 */

/**
 * @brief Manejador de catástrofes; bloquea la unidad tras errores fatales.
 */
void Error_Handler(void) {
  __disable_irq();
  while (1) { }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
