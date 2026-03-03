/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  main.c - Sistema de Alarma Vecinal
  Loop principal con ejecutor ciclico de 1ms.
  Cada modulo tiene su FSM y corre una vez por tick de SysTick.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "app_events.h"
#include "sensor_boton.h"
#include "sensor_ldr.h"
#include "sensor_ble.h"
#include "sensor_gsm.h"
#include "gsm_driver.h"
#include "eeprom_driver.h"
#include "sistema.h"
#include "act_sirena.h"
#include "act_estrobo.h"
#include "act_led_rojo.h"
#include "act_led_amarillo.h"
#include "sms_manager.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */

/* Flag seteado por SysTick cada 1 ms para disparar el ciclo de tareas. */
volatile uint8_t g_tick_flag = 0;

/* Profiling con DWT. Agregar en Live Expressions del debugger.
   t_xxx_us = ultimo tiempo de cada tarea en us
   wcet_xxx_us = maximo historico desde el arranque
   U = suma de todos los wcet_xxx / 1000 */
volatile uint32_t t_sensor_boton_us     = 0;
volatile uint32_t t_sensor_ldr_us       = 0;
volatile uint32_t t_sensor_ble_us       = 0;
volatile uint32_t t_sensor_gsm_us       = 0;
volatile uint32_t t_gsm_driver_us       = 0;
volatile uint32_t t_sistema_us          = 0;
volatile uint32_t t_act_sirena_us       = 0;
volatile uint32_t t_act_estrobo_us      = 0;
volatile uint32_t t_act_led_rojo_us     = 0;
volatile uint32_t t_act_led_amarillo_us = 0;
volatile uint32_t t_sms_manager_us      = 0;
volatile uint32_t t_eeprom_driver_us    = 0;

volatile uint32_t wcet_sensor_boton_us     = 0;
volatile uint32_t wcet_sensor_ldr_us       = 0;
volatile uint32_t wcet_sensor_ble_us       = 0;
volatile uint32_t wcet_sensor_gsm_us       = 0;
volatile uint32_t wcet_gsm_driver_us       = 0;
volatile uint32_t wcet_sistema_us          = 0;
volatile uint32_t wcet_act_sirena_us       = 0;
volatile uint32_t wcet_act_estrobo_us      = 0;
volatile uint32_t wcet_act_led_rojo_us     = 0;
volatile uint32_t wcet_act_led_amarillo_us = 0;
volatile uint32_t wcet_sms_manager_us      = 0;
volatile uint32_t wcet_eeprom_driver_us    = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */

/* Mide cuantos us tarda cada tarea con el DWT (72 ciclos = 1 us @ 72 MHz). */
#define TASK_MEASURE(t_var, wcet_var, func_call)    \
    do {                                             \
        uint32_t _c0 = DWT->CYCCNT;                 \
        func_call;                                   \
        uint32_t _dt = (DWT->CYCCNT - _c0) / 72U;  \
        (t_var) = _dt;                               \
        if (_dt > (wcet_var)) (wcet_var) = _dt;     \
    } while (0)

/* USER CODE END PFP */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();

    /* USER CODE BEGIN 2 */

    /* Activa el contador DWT para medir tiempos con TASK_MEASURE. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* Inicializacion de modulos. La EEPROM carga los vecinos antes del loop. */
    eeprom_driver_init();
    gsm_driver_init();

    sensor_boton_init();
    sensor_ldr_init();
    sensor_ble_init();
    sensor_gsm_init();

    sistema_init();
    sms_manager_init();

    act_sirena_init();
    act_estrobo_init();
    act_led_rojo_init();
    act_led_amarillo_init();

    /* USER CODE END 2 */

    /* Espera el tick de 1ms antes de correr las tareas. */
    while (1)
    {
        if (g_tick_flag) {
            g_tick_flag = 0;

            /* SENSE */
            TASK_MEASURE(t_sensor_boton_us,     wcet_sensor_boton_us,     sensor_boton_update());
            TASK_MEASURE(t_sensor_ldr_us,       wcet_sensor_ldr_us,       sensor_ldr_update());
            TASK_MEASURE(t_sensor_ble_us,       wcet_sensor_ble_us,       sensor_ble_update());
            TASK_MEASURE(t_sensor_gsm_us,       wcet_sensor_gsm_us,       sensor_gsm_update());
            TASK_MEASURE(t_gsm_driver_us,       wcet_gsm_driver_us,       gsm_driver_update());

            /* PROCESS */
            TASK_MEASURE(t_sistema_us,          wcet_sistema_us,          sistema_update());

            /* ACT */
            TASK_MEASURE(t_act_sirena_us,       wcet_act_sirena_us,       act_sirena_update());
            TASK_MEASURE(t_act_estrobo_us,      wcet_act_estrobo_us,      act_estrobo_update());
            TASK_MEASURE(t_act_led_rojo_us,     wcet_act_led_rojo_us,     act_led_rojo_update());
            TASK_MEASURE(t_act_led_amarillo_us, wcet_act_led_amarillo_us, act_led_amarillo_update());
            TASK_MEASURE(t_sms_manager_us,      wcet_sms_manager_us,      sms_manager_update());
            TASK_MEASURE(t_eeprom_driver_us,    wcet_eeprom_driver_us,    eeprom_driver_update());
        }

        /* Sleep mode */
        __WFI();
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

/* USER CODE BEGIN 4 */

/* El callback de SysTick levanta el flag que dispara el ciclo de tareas. */
void HAL_SYSTICK_Callback(void) {
    g_tick_flag = 1;
}

/* Los callbacks de recepcion UART despachan al modulo correspondiente segun
   el periferico que genero la interrupcion. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) sensor_ble_rx_callback();
    if (huart->Instance == USART3) sensor_gsm_rx_callback();
}

/* El callback de transmision completa notifica al motor SMS para que avance
   su FSM al siguiente estado. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) sms_manager_tx_done_callback();
}

/* El callback de escritura I2C (Mem_Write_IT completado) notifica al
   driver de EEPROM para que avance su FSM al siguiente estado. */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) flag_i2c_done = 1;
}

/* En caso de error UART se rearma la recepcion por interrupcion sin
   alterar el estado de ninguna FSM. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) sensor_ble_error_callback();
    if (huart->Instance == USART3) sensor_gsm_error_callback();
}

/* USER CODE END 4 */

void Error_Handler(void) {
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
