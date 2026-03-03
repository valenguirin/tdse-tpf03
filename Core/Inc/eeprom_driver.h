/* Driver EEPROM AT24C256 por I2C. La init() carga los vecinos al arranque,
   las escrituras desde el loop son no bloqueantes usando I2C IT. */
#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

void eeprom_driver_init(void);
void eeprom_driver_update(void);
void eeprom_driver_escribir_slot(int indice);
void eeprom_agregar_numero(char *numero);
void eeprom_borrar_numero(char *numero);

#endif /* EEPROM_DRIVER_H */
