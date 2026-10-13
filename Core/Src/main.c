/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hcsr04.h"
#include "ssd1315.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_MEASUREMENT_PERIOD_MS  200U
#define APP_OLED_RETRY_PERIOD_MS  1000U
#define APP_SELF_TEST_OK_TIME_MS   1000U
#define APP_PAGE_FLASH_TIME_MS      200U
#define APP_UART_TIMEOUT_MS         100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t s_oled_ready;
static uint32_t s_oled_last_retry_ms;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void App_Init(void);
static void App_Process(void);
static void App_SendUartResult(const HCSR04_Result *measurement);
static void App_UpdateOled(const HCSR04_Result *measurement);
static void App_RunStartupSelfTest(void);
static uint8_t App_ShowSelfTestPage(HCSR04_Status sensor_status,
                                    uint8_t checking);
static const char *App_GetSensorStatusText(HCSR04_Status status);
static const char *App_GetTrackingState(const HCSR04_Result *measurement);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Converts the sensor self-test status to a display string.
  */
static const char *App_GetSensorStatusText(HCSR04_Status status)
{
  switch (status)
  {
    case HCSR04_OK:
      return "SENSOR: OK";
    case HCSR04_TIMEOUT:
    case HCSR04_OUT_OF_RANGE:
      return "SENSOR: ERROR";
    case HCSR04_ECHO_STUCK_HIGH:
      return "SENSOR: ECHO HIGH";
    case HCSR04_TIMER_ERROR:
    default:
      return "SENSOR: TIMER ERR";
  }
}

/**
  * @brief  Classifies the latest valid distance for the tracking task.
  */
static const char *App_GetTrackingState(const HCSR04_Result *measurement)
{
  if ((measurement == NULL) || (measurement->status != HCSR04_OK))
  {
    return "--";
  }

  if ((measurement->distance_mm <= 30U) ||
      (measurement->distance_mm > 1000U))
  {
    return "--";
  }
  if (measurement->distance_mm < 150U)
  {
    return "CLOSE";
  }
  if (measurement->distance_mm < 250U)
  {
    return "OK";
  }
  if (measurement->distance_mm < 500U)
  {
    return "FAR";
  }

  return "FIND";
}

/**
  * @brief  Renders one complete measurement page in the OLED framebuffer.
  * @retval 1 when the I2C transfer succeeds, otherwise 0.
  */
static uint8_t App_ShowMeasurement(const HCSR04_Result *measurement)
{
  char distance_line[22];
  char echo_line[22];
  char state_line[22];

  if ((measurement == NULL) || (SSD1315_IsReady() == 0U))
  {
    return 0U;
  }

  if (measurement->status == HCSR04_OK)
  {
    (void)snprintf(distance_line, sizeof(distance_line), "DIST: %lu.%01lu CM",
                   (unsigned long)(measurement->distance_mm / 10U),
                   (unsigned long)(measurement->distance_mm % 10U));
  }
  else
  {
    (void)snprintf(distance_line, sizeof(distance_line), "DIST: --.- CM");
  }

  (void)snprintf(echo_line, sizeof(echo_line), "ECHO: %lu US",
                 (unsigned long)measurement->echo_time_us);
  (void)snprintf(state_line, sizeof(state_line), "STATE: %s",
                 App_GetTrackingState(measurement));

  SSD1315_Clear();
  SSD1315_SetCursor(0U, 0U);
  SSD1315_WriteString("HC-SR04 TEST");
  SSD1315_SetCursor(0U, 16U);
  SSD1315_WriteString(distance_line);
  SSD1315_SetCursor(0U, 32U);
  SSD1315_WriteString(echo_line);
  SSD1315_SetCursor(0U, 48U);
  SSD1315_WriteString(state_line);

  return SSD1315_Update();
}

/**
  * @brief  Renders the startup self-test page.
  */
static uint8_t App_ShowSelfTestPage(HCSR04_Status sensor_status,
                                    uint8_t checking)
{
  if (SSD1315_IsReady() == 0U)
  {
    return 0U;
  }

  SSD1315_Clear();
  SSD1315_SetCursor(0U, 0U);
  SSD1315_WriteString("HC-SR04 TEST");
  SSD1315_SetCursor(0U, 16U);
  SSD1315_WriteString((checking != 0U) ? "STATUS: CHECK" :
                      ((sensor_status == HCSR04_OK) ? "STATUS: OK" :
                                                     "STATUS: ERROR"));
  SSD1315_SetCursor(0U, 32U);
  SSD1315_WriteString("OLED: OK");
  SSD1315_SetCursor(0U, 48U);
  SSD1315_WriteString((checking != 0U) ? "SENSOR: CHECK" :
                                        App_GetSensorStatusText(sensor_status));

  return SSD1315_Update();
}

/**
  * @brief  Waits for the OLED and basic ultrasonic hardware checks to pass.
  */
static void App_RunStartupSelfTest(void)
{
  HCSR04_Status sensor_status;

  for (;;)
  {
    if (SSD1315_IsReady() == 0U)
    {
      s_oled_ready = SSD1315_Init();
      if (s_oled_ready == 0U)
      {
        static const char oled_error[] = "self_test:OLED_ERR\r\n";
        (void)HAL_UART_Transmit(&huart1, (const uint8_t *)oled_error,
                                (uint16_t)(sizeof(oled_error) - 1U),
                                APP_UART_TIMEOUT_MS);
        HAL_Delay(APP_OLED_RETRY_PERIOD_MS);
        continue;
      }
    }

    if (App_ShowSelfTestPage(HCSR04_OK, 1U) == 0U)
    {
      s_oled_ready = 0U;
      continue;
    }

    HCSR04_Init();
    sensor_status = HCSR04_SelfTest();
    if (App_ShowSelfTestPage(sensor_status, 0U) == 0U)
    {
      s_oled_ready = 0U;
      continue;
    }

    if (sensor_status == HCSR04_OK)
    {
      static const char self_test_ok[] = "self_test:OK\r\n";
      (void)HAL_UART_Transmit(&huart1, (const uint8_t *)self_test_ok,
                              (uint16_t)(sizeof(self_test_ok) - 1U),
                              APP_UART_TIMEOUT_MS);
      HAL_Delay(APP_SELF_TEST_OK_TIME_MS);
      SSD1315_Clear();
      s_oled_ready = SSD1315_Update();
      HAL_Delay(APP_PAGE_FLASH_TIME_MS);
      return;
    }

    {
      char self_test_line[48];
      const int length = snprintf(self_test_line, sizeof(self_test_line),
                                  "self_test:%s\r\n",
                                  App_GetSensorStatusText(sensor_status));
      if (length > 0)
      {
        const uint16_t transmit_length =
            (length < (int)sizeof(self_test_line)) ?
            (uint16_t)length : (uint16_t)(sizeof(self_test_line) - 1U);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)self_test_line,
                                transmit_length, APP_UART_TIMEOUT_MS);
      }
    }

    HAL_Delay(APP_OLED_RETRY_PERIOD_MS);
  }
}

/**
  * @brief  Initializes the application drivers after CubeMX peripherals.
  */
static void App_Init(void)
{
  static const char banner[] =
      "\r\nHC-SR04 GPIO test ready (USART1 9600 8N1)\r\n";

  (void)HAL_UART_Transmit(&huart1, (const uint8_t *)banner,
                          (uint16_t)(sizeof(banner) - 1U),
                          APP_UART_TIMEOUT_MS);

  s_oled_ready = 0U;
  App_RunStartupSelfTest();
  s_oled_last_retry_ms = HAL_GetTick();
}

/**
  * @brief  Sends one measurement in a serial-terminal-friendly format.
  */
static void App_SendUartResult(const HCSR04_Result *measurement)
{
  char line[96];
  int length;

  if (measurement == NULL)
  {
    return;
  }

  if (measurement->status == HCSR04_OK)
  {
    length = snprintf(line, sizeof(line),
                      "distance_cm:%lu.%01lu,echo_us:%lu,status:%u,state:%s\r\n",
                      (unsigned long)(measurement->distance_mm / 10U),
                      (unsigned long)(measurement->distance_mm % 10U),
                      (unsigned long)measurement->echo_time_us,
                      (unsigned int)measurement->status,
                      App_GetTrackingState(measurement));
  }
  else
  {
    length = snprintf(line, sizeof(line),
                      "distance_cm:0.0,echo_us:%lu,status:%u,state:--\r\n",
                      (unsigned long)measurement->echo_time_us,
                      (unsigned int)measurement->status);
  }

  if (length > 0)
  {
    const uint16_t transmit_length =
        (length < (int)sizeof(line)) ?
        (uint16_t)length : (uint16_t)(sizeof(line) - 1U);

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, transmit_length,
                            APP_UART_TIMEOUT_MS);
  }
}

/**
  * @brief  Updates the OLED and periodically reconnects a missing display.
  */
static void App_UpdateOled(const HCSR04_Result *measurement)
{
  const uint32_t now = HAL_GetTick();

  if (s_oled_ready != 0U)
  {
    s_oled_ready = App_ShowMeasurement(measurement);
  }
  else if ((now - s_oled_last_retry_ms) >= APP_OLED_RETRY_PERIOD_MS)
  {
    s_oled_last_retry_ms = now;
    s_oled_ready = SSD1315_Init();
    if (s_oled_ready != 0U)
    {
      s_oled_ready = App_ShowMeasurement(measurement);
    }
  }
}

/**
  * @brief  Performs one measurement, report and display cycle.
  */
static void App_Process(void)
{
  const HCSR04_Result measurement = HCSR04_Measure();

  App_SendUartResult(&measurement);
  App_UpdateOled(&measurement);
  HAL_Delay(APP_MEASUREMENT_PERIOD_MS);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  App_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    App_Process();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
