/*
 * eeprom_driver.c
 *
 * Driver para la memoria EEPROM externa conectada por I2C.
 *
 * La EEPROM guarda la lista de numeros autorizados de vecinos. Al arrancar,
 * eeprom_driver_init() lee todos los slots a la RAM (operacion bloqueante,
 * ocurre una sola vez antes del loop principal). Si la memoria es nueva o
 * esta corrupta, se formatea y se escribe un numero de vecino por defecto.
 *
 * Las escrituras desde el loop (alta y baja de numeros) son no bloqueantes.
 * eeprom_driver_escribir_slot() encola la operacion y eeprom_driver_update()
 * la ejecuta un byte por tick, con una espera de 5 ms entre bytes usando
 * HAL_GetTick() en lugar de HAL_Delay().
 *
 * Mapa de memoria:
 *   0x0000         : MAGIC_BYTE (0xAB), indica que la EEPROM tiene datos validos.
 *   0x0010 + i*16  : slot del vecino i (15 bytes de numero + terminador).
 */

#include "eeprom_driver.h"
#include "app_events.h"
#include "main.h"
#include "i2c.h"
#include <string.h>

#define EEPROM_ADDR             0xA0U
#define MAGIC_BYTE              0xABU
#define TAMAÑO_DIRECCION_EEPROM I2C_MEMADD_SIZE_16BIT
#define EE_WRITE_CYCLE_MS       5U

typedef enum {
    EE_IDLE,       /* Sin operaciones pendientes. */
    EE_WRITING,    /* Escribe un byte en la EEPROM. */
    EE_WAIT_CYCLE  /* Espera los 5 ms que requiere el chip entre escrituras. */
} ee_state_t;

static ee_state_t ee_state    = EE_IDLE;
static int        ee_slot     = -1;
static int        ee_byte_idx = 0;
static bool       ee_is_del   = false;
static uint32_t   ee_tick     = 0;

/* Escritura bloqueante de un slot; solo se usa en eeprom_driver_init(). */
static void escribir_slot_blocking(int indice) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDR, 2, 50) != HAL_OK) return;

    uint16_t addr = 16U + (uint16_t)(indice * 16);

    if (lista_vecinos[indice][0] == '\0') {
        uint8_t vacio = '\0';
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, addr,
                          TAMAÑO_DIRECCION_EEPROM, &vacio, 1, 50);
        HAL_Delay(EE_WRITE_CYCLE_MS);
        return;
    }

    for (int j = 0; j < 15; j++) {
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, addr + (uint16_t)j,
                          TAMAÑO_DIRECCION_EEPROM,
                          (uint8_t *)&lista_vecinos[indice][j], 1, 50);
        HAL_Delay(EE_WRITE_CYCLE_MS);
        if (lista_vecinos[indice][j] == '\0') break;
    }
}

void eeprom_driver_init(void) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDR, 3, 100) != HAL_OK) return;

    uint8_t check_byte = 0;
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0,
                     TAMAÑO_DIRECCION_EEPROM, &check_byte, 1, 100);

    if (check_byte == MAGIC_BYTE) {
        /* La EEPROM tiene datos previos: se cargan los 200 slots a RAM. */
        for (int i = 0; i < MAX_VECINOS; i++) {
            uint16_t mem_addr = 16U + (uint16_t)(i * 16);
            HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, mem_addr,
                             TAMAÑO_DIRECCION_EEPROM,
                             (uint8_t *)lista_vecinos[i], 15, 10);
            lista_vecinos[i][14] = '\0';
        }
    } else {
        /* Primera vez: se escribe el magic byte y se limpian todos los slots.
           El HAL_Delay aqui es aceptable porque ocurre una sola vez en la
           vida del dispositivo y antes de que arranque el loop principal. */
        check_byte = MAGIC_BYTE;
        HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, 0,
                          TAMAÑO_DIRECCION_EEPROM, &check_byte, 1, 100);
        HAL_Delay(10);

        for (int i = 0; i < MAX_VECINOS; i++) lista_vecinos[i][0] = '\0';

        uint8_t vacio = '\0';
        for (int i = 0; i < MAX_VECINOS; i++) {
            uint16_t mem_addr = 16U + (uint16_t)(i * 16);
            HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, mem_addr,
                              TAMAÑO_DIRECCION_EEPROM, &vacio, 1, 50);
            HAL_Delay(EE_WRITE_CYCLE_MS);
        }

        strncpy(lista_vecinos[0], "1133588475", 14);
        lista_vecinos[0][14] = '\0';
        escribir_slot_blocking(0);
    }
}

/* Encola la escritura no bloqueante de un slot.
   Si el driver esta ocupado con otra escritura, la peticion se descarta.
   Esto no es un problema porque los comandos BLE llegan de a uno. */
void eeprom_driver_escribir_slot(int indice) {
    if (ee_state != EE_IDLE) return;
    ee_slot     = indice;
    ee_byte_idx = 0;
    ee_is_del   = (lista_vecinos[indice][0] == '\0');
    ee_state    = EE_WRITING;
}

void eeprom_driver_update(void) {
    switch (ee_state) {

        case EE_IDLE:
            break;

        case EE_WRITING: {
            uint16_t addr = 16U + (uint16_t)(ee_slot * 16) + (uint16_t)ee_byte_idx;
            HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, addr,
                              TAMAÑO_DIRECCION_EEPROM,
                              (uint8_t *)&lista_vecinos[ee_slot][ee_byte_idx], 1, 50);
            ee_tick  = HAL_GetTick();
            ee_state = EE_WAIT_CYCLE;
            break;
        }

        case EE_WAIT_CYCLE:
            if ((HAL_GetTick() - ee_tick) >= EE_WRITE_CYCLE_MS) {
                bool done;
                if (ee_is_del) {
                    done = true;  /* Para borrado solo se escribe el primer byte. */
                } else {
                    done = (ee_byte_idx >= 14 ||
                            lista_vecinos[ee_slot][ee_byte_idx] == '\0');
                }
                if (done) {
                    ee_state = EE_IDLE;
                } else {
                    ee_byte_idx++;
                    ee_state = EE_WRITING;
                }
            }
            break;
    }
}

void eeprom_agregar_numero(char *numero) {
    for (int i = 0; i < MAX_VECINOS; i++) {
        if (lista_vecinos[i][0] == '\0') {
            strncpy(lista_vecinos[i], numero, 14);
            lista_vecinos[i][14] = '\0';
            eeprom_driver_escribir_slot(i);
            return;
        }
    }
}

void eeprom_borrar_numero(char *numero) {
    for (int i = 0; i < MAX_VECINOS; i++) {
        if (strcmp(lista_vecinos[i], numero) == 0) {
            memset(lista_vecinos[i], 0, 15);
            eeprom_driver_escribir_slot(i);
            return;
        }
    }
}
