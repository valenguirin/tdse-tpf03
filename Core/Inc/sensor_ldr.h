/**
 * @file  sensor_ldr.h
 * @brief FSM del LDR para detección de ciclo día/noche (ventana 2 s).
 */
#ifndef SENSOR_LDR_H
#define SENSOR_LDR_H

void sensor_ldr_init(void);
void sensor_ldr_update(void);

#endif /* SENSOR_LDR_H */
