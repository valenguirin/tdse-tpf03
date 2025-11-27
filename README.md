## ![](logo-fiuba.png)

# Alarma vecinal

### Autores: Valentín Guirin, Carolina Gonzales Peralta, Yerson Michael Monzón Alayo
### Padrones: 107416, 110804, 104262

### Fecha: 2° cuatrimestre de 2025

## Selección del proyecto a implementar

### Contexto.

El proyecto a implementar tiene sus bases en la inseguridad que hoy en día está presente en la Ciudad
de Buenos Aires. En particular, en las villas miseria, en donde la inseguridad es recurrente
y firma de estos lugares. Dadas estas circunstancias, el cuidado entre vecinos residentes
es crucial y creemos que la adquisición de una alarma vecinal como la que se propone en este
informe, es importante y sugiere que sería un producto potencialmente comercial. Si bien ya
existen este tipo de mecanismos, los vecinos tienden a organizarse informalmente: grupos de
mensajería, silbatos, bocinas caseras o campanas. Sin embargo, estos mecanismos suelen ser
descoordinados, no escalables y dependen de que un vecino en particular esté atento, tenga
crédito en el celular o pueda emitir un mensaje en el momento justo.
El presente trabajo final propone el diseño e implementación de un sistema de alarma
vecinal basado en una plataforma de sistemas embebidos. El objetivo es que varios vecinos
autorizados puedan activar de forma remota una sirena común mediante una llamada telefónica sin
costo (llamada no contestada) y que el sistema notifique el evento al resto de la comunidad mediante
SMS y Bluetooth Low Energy (BLE).

### Objetivo del proyecto y resultados esperados

Diseñar un nodo de alarma vecinal, instalado en la calle, que pueda ser activado de forma
remota por vecinos autorizados, sin costo por llamada, y que a su vez:
- Permita una administración local de la configuración (números autorizados, tiempos de
sirena) mediante teclado y LCD.
- Provea feedback al resto de los vecinos mediante SMS y un canal BLE.
- Cumpla restricciones de bajo consumo, robustez y simplicidad de uso propias de un
sistema embebido sin sistema operativo.
En términos técnicos, se busca materializar un sistema ciberfísico capaz de:
- Escuchar eventos externos (llamadas GSM, teclas, BLE, sensores).
- Procesarlos mediante una máquina de estados bien definida.
- Actuar sobre una sirena, buzzer, LEDs y canales de comunicación, de forma determinista
y medible

### Descripción de alto nivel.

El sistema consiste en un nodo de alarma vecinal compuesto por los siguientes elementos
principales:
- Placa NUCLEO-F103RB (STM32F103RB) como unidad de procesamiento central.
- Módulo GSM SIM800L para recepción de llamadas y envío de SMS.
- Módulo BLE HM-10 para exposición de estado hacia teléfonos móviles a través de una app
tipo “terminal BLE”.
- Teclado matricial 4x4 y LCD 16x2 para interacción local (menú, PIN de administrador,
gestión de lista blanca).
- Módulo de relé para comando de una sirena o luz estroboscópica de mayor potencia.
- Buzzer piezoeléctrico para feedback sonoro local.
- LEDs de estado (ARMADA, ALARMA, FALLA).
- Memoria no volátil (EEPROM I2C externa o Flash interna) para almacenar parámetros de
configuración (SET_UP), en particular la lista blanca de teléfonos.
- Sensor analógico (por ejemplo, LM35 o NTC) para monitoreo de temperatura interna de
gabinete y soporte al modo FALLA.
- Dip switches para selección de perfil de funcionamiento (modo demo/real, número de nodo, etc.)

### Descripción desde el punto de vista funcional.

- Recibe una llamada entrante en el SIM card de la alarma.
- Obtiene el número llamante (Caller ID) vía comandos AT y lo compara contra una lista
blanca.
- Si el número está autorizado, corta la llamada sin contestar (no se factura) y activa la
sirena durante un tiempo configurable.
- Opcionalmente, envía SMS de alerta a un conjunto de teléfonos predefinidos.
- Permite activar la sirena localmente mediante un botón de pánico en el teclado.
- Expone el estado del sistema por BLE (HM-10) para que los vecinos puedan consultar si la
alarma está armada/desarmada y cuáles fueron los últimos eventos.
- Permite que un administrador, mediante PIN, gestione la lista blanca y parámetros desde el
LCD/teclado.

### Alcance del MVP.

Para acotar el trabajo y cumplir con los plazos de la materia, se define un MVP con el
siguiente alcance:
- Soporte de hasta N números de teléfono en lista blanca.
- Un único nodo de alarma, instalado en una esquina o edificio.
- Activación de la sirena por:
 - llamada entrante autorizada,
 - botón de pánico local en el teclado.
- Notificación de evento vía:
 - SMS a al menos un número predefinido,
 - mensaje de texto por BLE (estado + último evento).
- Un único sensor analógico para diagnóstico (temperatura interna en caso de que la emergencia sea incendio).
- Al menos dos modos de operación implementados y demostrables: modo NORMAL, SET_UP y FALLA.

### Algunos componentes y funciones.

-Botones/Teclas: teclado matricial 4x4, tecla de pánico, navegación de menú
- LEDs: Alarma, falla
- Buzzer: feedback sonoto en cambios de estado, errores y pulsaciones
- Memoria no volátil: EEPROM I2C externa o Flash interna para lista blanca y parámetros
- Sensor analógico: LM35/NTC para diagnóstico térmico
- Dip switches: selección de perfil de funcionamiento y nodo
- HM-10: canal de monitoreo BLE para estado y último evento

### Componentes principales.

- NUCLEO-F103RB (STM32F103RB).
- Módulo GSM SIM800L con fuente regulada a ~4,0 V y capacidad de al menos 2 A.
- Módulo BLE HM-10 alimentado a 3,3 V.
- LCD 16x2 compatible HD44780 en modo 4 bits.
- Teclado matricial 4x4.
- Módulo de relé con aislamiento óptico para la sirena/luz.
- Buzzer piezoeléctrico 5 V.
- LEDs indicadores (mínimo 3: ARM, ALARM, FAIL).
- Sensor analógico (LM35/NTC).
- EEPROM I2C (opcional) o uso de Flash interna.
- Dip switches (al menos 2 bits de configuración).
- Fuente de alimentación con dos etapas de regulación

## Diagrama en bloques del sistema

![Diagrama en bloques del sistema](diagrama_de_bloques.jpg)

## Elicitación de requisitos y casos de uso

En la Ciudad de Buenos Aires existe un competidor crucial en el mercado de la seguridad interconectada: [Verisure](https://www.verisure.com.ar/blog/alarma-barrial-que-es). Si bien es una marca de un gran calibre, consideramos que nuestro proyecto se enfoca en una zona particular y muy específica de la ciudad, dándonos la posibilidad de poder adaptar nuestro producto a las necesidades de la gente que allí resida y así poder consolidarnos en el mercado. Nuestra gran diferencia con
Verisure es que gran parte de nuestro trabajo será para reducir los costos al mínimo dado el público comprador. Evaluaremos en el transcurso del proyecto si
convendría vender al Gobierno de la Ciudad o directamente a los residentes, pero concluimos que esta cuestión no influye en los requerimientos del producto
ya que mantener los costos al mínimo y las funcionalidades que han sido mencionadas son cuestiones que mantendremos independientemente de si la alarma vecinal
llega a manos de los compradores a través del gobierno o no.

Cabe destacar que, si bien Verisure es nuestro competidos de mayor escala, actualmente hay otras empresas que se dedican a fabricar alarmas no vecinales pero
que tienen el potencial como para hacerlo. En ese caso, habría más competencia pero creemos que si logramos enfocarnos en las prioridades del costo y
funcionalidades, podremos hacernos con parte de la ciudad.





