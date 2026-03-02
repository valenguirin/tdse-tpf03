/*
 * sistema.c
 *
 * Maquina de estados central del sistema de alarma vecinal.
 * Lee el bus de eventos, toma decisiones y emite comandos a los actuadores.
 * No accede directamente a ningun pin de hardware.
 *
 * Estados:
 *   SYS_STANDBY     : monitoreo continuo de eventos de panico.
 *   SYS_ALARM_ACTIVE: coordina la emergencia hasta que se vacia la cola de SMS.
 *
 * Al detectar un evento de panico activa la sirena, bloquea la sesion BLE,
 * encola los SMS de alerta y, si es de noche, activa el estrobo.
 */
#include "sistema.h"
#include "sms_manager.h"
#include "app_events.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    SYS_STANDBY,
    SYS_ALARM_ACTIVE
} fsm_sys_state_t;

static fsm_sys_state_t fsm_sys = SYS_STANDBY;

void sistema_init(void) {
    fsm_sys = SYS_STANDBY;
}

void sistema_update(void) {
    /* El estado dia/noche se actualiza desde el bus antes de evaluar alarmas. */
    if (ev_sys_night_mode) { is_night = true;  ev_sys_night_mode = false; }
    if (ev_sys_day_mode)   { is_night = false; ev_sys_day_mode   = false; }

    switch (fsm_sys) {

        case SYS_STANDBY:
            if (ev_panico_boton || ev_panico_llamada) {
                fsm_sys         = SYS_ALARM_ACTIVE;
                sys_ocupado_sms = true;
                tick_delay_red  = HAL_GetTick();

                cmd_siren_on       = true;
                cmd_ble_force_lock = true;   /* Cierra la sesion BLE durante la emergencia. */
                cmd_led_rojo_off   = true;

                /* El estrobo solo se activa en modo nocturno. */
                if (is_night) {
                    cmd_strobe_on = true;
                }

                /* Los SMS se encolan segun el origen del evento de panico. */
                if (ev_panico_boton) {
                    sms_manager_generar_ruta("ALARMA BTN PANICO");
                } else if (ev_panico_llamada) {
                    sms_manager_generar_ruta("ALARMA POR LLAMADA");
                    /* Se notifica a cada central el numero de telefono activador. */
                    for (int i = 0; i < MAX_CENTRALES; i++) {
                        char msg_num[50];
                        snprintf(msg_num, sizeof(msg_num),
                                 "Activado por la linea: %s", ev_numero_activador);
                        sms_manager_encolar(lista_centrales[i], msg_num);
                    }
                }
            }
            break;

        case SYS_ALARM_ACTIVE:
            /* El sistema vuelve a standby cuando el gestor de SMS vacia la cola. */
            if (ev_sys_sms_done) {
                sys_ocupado_sms = false;
                fsm_sys         = SYS_STANDBY;
                cmd_led_rojo_on = true;
                ev_sys_sms_done = false;
            }

            if (ev_silenciar) {
                cmd_siren_off  = true;
                cmd_strobe_off = true;
            }
            break;
    }

    /* Los eventos se consumen al final para que sistema sea el unico lector. */
    ev_panico_boton   = false;
    ev_panico_llamada = false;
    ev_silenciar      = false;
}
