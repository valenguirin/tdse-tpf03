# Análisis de Ejecución y Consumo Energético - Sistema de Alarma Vecinal

El presente documento detalla las métricas de rendimiento y el perfil de consumo eléctrico del sistema. La evaluación responde 
a los requerimientos técnicos de la arquitectura de hardware y software del proyecto.

## 1. Medición y análisis de consumo

Las pruebas de consumo de corriente sobre la placa NUCLEO-F103RB arrojaron los siguientes resultados. 
El entorno de prueba mantuvo el módulo Bluetooth (BLE) conectado al stm a 5V permanente durante todas las mediciones.

* **Consumo sobre la línea de 5 V (Sistema general):**
    * Estado de reposo: **36.1 mA**
    * Estado activo: **37.8 mA**
    * *Evidencia 1:*
<div align="center">
  <img src="img/01_consumo_5v.jpeg" alt="Medición de consumo a 5V" width="600">
</div>
<div align="center">
  <img src="img/02_consumo_5v.jpeg" alt="Medición de consumo a 5V" width="600">
</div>

* **Consumo sobre la línea de 3.3 V (Microcontrolador STM32):**
    * Estado de reposo: **31.1 mA**
    * Estado activo: **33.1 mA**
    * *Evidencia 2:*
 
<div align="center">
  <img src="img/01_consumo_3v3.jpeg" alt="Medición de consumo a 3.3V" width="600">
</div>

<div align="center">
  <img src="img/02_consumo_3v3.jpeg" alt="Medición de consumo a 3.3V" width="600">
</div>

### Análisis del módulo GSM (SIM800L)
Como secarece de un osciloscopio. Por este motivo, el reporte omite la medición 
de los picos transitorios de consumo del módulo GSM SIM800L. 

Para un registro preciso de la energía de este componente, el procedimiento teórico sería 
la instalación de una resistencia  de bajo valor (por ejemplo, 0.1 ohmios) en serie con 
la alimentación positiva (VCC) del módulo. Un osciloscopio, con sus sondas en paralelo a la resistencia, 
se debería capturar la caída de tensión máxima originada por la ráfaga de transmisión de un SMS o llamda entrante. 
De esta forma el cálculo de la corriente pico real 
surge de la división de este voltaje máximo por el valor resistivo, por la Ley de Ohm.

## 2. Medición y análisis de tiempos de ejecución de cada tarea (WCET)

La evaluación temporal del *Worst Case Execution Time* (WCET) requiere la instrumentación de un pin GPIO y el registro del pulso lógico máximo.

* Tiempo máximo de ejecución registrado (WCET): **[COMPLETAR VALOR EN MILISEGUNDOS/MICROSEGUNDOS]**

* Condiciones de la prueba de estrés: **[DESCRIBIR BREVEMENTE QUÉ ALARMAS SE ACTIVARON PARA LA PRUEBA]**

* *Evidencia 3:*
<div align="center">
  <img src="img/03_wcet_osciloscopio.jpg" alt="Captura del pulso WCET" width="600">
</div>

## 3. Captura de pantalla de "Console & Build Analyzer"

El reporte de uso de memoria tras la compilación de la versión final del código fuente expone la distribución de recursos en la memoria FLASH y RAM.

* *Evidencia 4: Console*
<div align="center">
  <img src="img/04_console.png" alt="Captura de Console" width="600">
</div>

* *Evidencia 5: Build Analyzer*
<div align="center">
  <img src="img/05_build_analyzer.png" alt="Captura de Build Analyzer" width="600">
</div>

## 4. Cálculo del Factor de Uso (U) de la CPU

La determinación del factor de carga del procesador aplica la relación directa entre el tiempo de ejecución en el peor de los casos y la duración total del ciclo del sistema.

**Fórmula aplicada:**
$$U = \frac{WCET}{T_{ciclo}} \times 100$$

**Desarrollo del cálculo:**
* WCET: **[COMPLETAR VALOR]**

* Tiempo de ciclo total ($T_{ciclo}$): **[COMPLETAR VALOR DEL WCET + RETARDO DEL SISTEMA]**

* Resultado Final ($U$): **[COMPLETAR PORCENTAJE]%**

## 5. Gestión del modo de bajo consumo

El código fuente incorpora directrices específicas de administración de energía para optimizar el rendimiento térmico y eléctrico.

* **Modo seleccionado:** Sleep Mode (`HAL_PWR_EnterSLEEPMode`).

* **Justificación técnica:** El cálculo del factor de uso demuestra la inactividad de la unidad central de procesamiento la mayor parte del tiempo. El sistema demanda mantener operativas las interfaces UART para la recepción asíncrona de comandos desde los periféricos Bluetooth y GSM. El modo Sleep detiene el reloj del núcleo central, pero conserva los periféricos encendidos. El microcontrolador despierta automáticamente ante interrupciones de red o al completarse el ciclo del temporizador base.

* *Evidencia 6:*
<div align="center">
  <img src="img/06_codigo_sleep.png" alt="Código de bajo consumo" width="600">
</div>

```c
      /* Bloque de Transmisión: El motor UART avanza pasos en la negociación con la red. */
      FSM_SMS_Update();

      /* Entra en Sleep Mode. La CPU se detiene ahorrando energía, pero los periféricos UART
         siguen activos. El sistema despierta automáticamente con la interrupción del
         SysTick (1ms) o cuando ingresa un dato por Bluetooth/GSM. */
      HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  }
}
