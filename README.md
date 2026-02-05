<img src="logo-fiuba.png" width="50%" />

# Alarma vecinal

### Autores: Valentín Guirin, Carolina Gonzales Peralta, Yerson Michael Monzón Alayo
### Padrones: 107416, 110804, 104262

### Fecha: 2° cuatrimestre de 2025

## Estado de requisitos

### Noción de avance.

En la Tabla 1.2 se muestra el estado de avance de cada requisito propuesto en la selección del proyecto a implementar. Las referencias color - estado están dadas por la Tabla 1.1 a continuación.

| Color | Estado |
| :---- | :---- |
|🟢|Ya implementado|
|🟡|No se implementó, pero se implementará (o se implementó parcialmente)|
|🔴|No se implementará|


<p align="center"><em>Tabla 1.1: Asignación color - estado</em></p>

| Grupo | ID | Descripción | Estado | 
| :---- | :---- | :---- | :---- |
|Acceso|1.1|El sistema permitirá el acceso mediante BLE.|🟡|
||1.2|En caso de acceso permitido, el sistema guardará qué usuario root ingresó|🟡|
|Indicadores|2.1|El sistema contará con un indicador luminoso (luz estorboscópica) para indicar que hay una alerta.|🟡|
||2.2|El sistema contará con un buzzer (sirena) para indicar la activación de la alarma.|🟡|
||2.3|El sistema contará con un set de leds para indicar que la clave es correcta.|🟡|
||2.4|El sistema contará con un set de leds para indicar que la clave es incorrecta.|🟡|
||2.5|El sistema enviará un mensaje a la policía, a todos los usuarios y a la central mediante GSM para indicar qué usuario activó la alarma mediante llamada.|🟡|
||2.6|El sistema enviará un mensaje a la policía, a todos los usuarios y a la central mediante GSM para indicar que la alarma se activó mediante botón de pánico.|🟡|
||2.7|El sistema contará con un led para indicar el estado de la alarma (armada o desarmada).|🟡|
|Interruptores/Botones|3.1|El sistema contará con un botón para accionar la alarma de forma manual (botón de pánico).|🟡|
|Memoria|4.1|El sistema contará con una memoria para almacenar datos.|🟡|
||4.2|La memoria almacenará la lista de números telefónicos autorizados.|🟡|
||4.3|La memoria almacenará las coordenadas (configuradas por la central) de la ubicación de la alarma.|🟡|
|Comunicación audio|5.1|El sistema contará con un buzzer (sirena) para transmitir la alerta.|🟡|
|Comunicación BLE|6.1|El personal autorizado enviado por la central se vinculará con el sistema mediante BLE.|🟡|
|Comunicación GSM|7.1|El sistema se comunicará con los usuarios mediante la red GSM (vía SMS).|🟡|
||7.2|El sistema se comunicará con la policía mediante la red GSM (vía SMS).|🟡|
||7.3|El sistema se comunicará con la central mediante la red GSM (vía SMS).|🟡|
|Sensores|8.1|El sistema contará con un sensor lumínico para validar la luz de día.|🟡|
|Entrega|9.1|La entrega del proyecto está prevista para el mes de febrero de 2026.|🟢|

<p align="center"><em>Tabla 1.2: Estado de requisitos del proyecto</em></p>
