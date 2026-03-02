/**
 * @file  eeprom_driver.h
 * @brief Driver de EEPROM I2C con escritura no bloqueante.
 *
 * - eeprom_driver_init(): lee datos al arranque (bloqueante, una sola vez).
 * - eeprom_driver_escribir_slot(): encola la escritura sin HAL_Delay.
 * - eeprom_driver_update(): avanza la FSM de escritura (1 byte cada 5 ms).
 * - eeprom_agregar_numero() / eeprom_borrar_numero(): gestión de la BD de vecinos.
 */
#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

void eeprom_driver_init(void);
void eeprom_driver_update(void);
void eeprom_driver_escribir_slot(int indice);
void eeprom_agregar_numero(char *numero);
void eeprom_borrar_numero(char *numero);

#endif /* EEPROM_DRIVER_H */
