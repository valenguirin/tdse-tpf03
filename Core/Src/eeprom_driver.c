/*
 * eeprom_driver.c
 *
 * Driver para la EEPROM externa AT24C256 conectada por I2C.
 *
 * Al arrancar lee todos los numeros de vecinos a RAM (bloqueante, solo una vez
 * antes del loop). Si la memoria es nueva la formatea y pone un numero por defecto.
 *
 * Las escrituras desde el loop son no bloqueantes: usa I2C IT y espera
 * los 5ms que pide el chip entre bytes con HAL_GetTick().
 *
 * Mapa: 0x0000 = magic byte (0xAB), 0x0010 + i*16 = slot vecino i.
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
    EE_WRITING,    /* Inicia escritura no bloqueante de un byte por I2C IT. */
    EE_WAIT_I2C,   /* Espera que la ISR confirme TX I2C completado. */
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
        /* Primera vez: escribe el magic byte y limpia todos los slots.
           Los HAL_Delay aca estan bien porque esto pasa solo al primer arranque
           antes de que empiece el loop. */
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
            flag_i2c_done = 0;
            HAL_I2C_Mem_Write_IT(&hi2c1, EEPROM_ADDR, addr,
                                 TAMAÑO_DIRECCION_EEPROM,
                                 (uint8_t *)&lista_vecinos[ee_slot][ee_byte_idx], 1);
            ee_state = EE_WAIT_I2C;
            break;
        }

        case EE_WAIT_I2C:
            /* HAL_I2C_MemTxCpltCallback (main.c) setea flag_i2c_done = 1. */
            if (flag_i2c_done) {
                ee_tick  = HAL_GetTick();
                ee_state = EE_WAIT_CYCLE;
            }
            break;

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
