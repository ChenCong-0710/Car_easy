#include "hcsr04.h"
#include "tim.h"

#define HCSR04_TRIGGER_PULSE_US       15U
#define HCSR04_CAPTURE_TIMEOUT_MS     30U
#define HCSR04_MIN_DISTANCE_MM        20U
#define HCSR04_MAX_DISTANCE_MM        4000U

static uint8_t timer_ready;

static void HCSR04_DelayUs(uint32_t delay_us)
{
  const uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);

  while ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < delay_us)
  {
  }
}

static uint8_t HCSR04_WaitForCapture(uint32_t flag, uint32_t timeout_ms)
{
  const uint32_t start = HAL_GetTick();

  while (__HAL_TIM_GET_FLAG(&htim2, flag) == RESET)
  {
    if ((HAL_GetTick() - start) >= timeout_ms)
    {
      return 0U;
    }
  }

  return 1U;
}

void HCSR04_Init(void)
{
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
  timer_ready = 0U;

  /* Keep initialization repeatable for the startup self-test retry path. */
  (void)HAL_TIM_IC_Stop(&htim2, TIM_CHANNEL_2);
  (void)HAL_TIM_IC_Stop(&htim2, TIM_CHANNEL_1);

  if (HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
    return;
  }

  if (HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
  {
    (void)HAL_TIM_IC_Stop(&htim2, TIM_CHANNEL_1);
    return;
  }

  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC1 | TIM_FLAG_CC2);
  timer_ready = 1U;
}

HCSR04_Status HCSR04_SelfTest(void)
{
  uint32_t counter_start;

  if (timer_ready == 0U)
  {
    return HCSR04_TIMER_ERROR;
  }

  counter_start = __HAL_TIM_GET_COUNTER(&htim2);
  HAL_Delay(1U);
  if (__HAL_TIM_GET_COUNTER(&htim2) == counter_start)
  {
    return HCSR04_TIMER_ERROR;
  }

  if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
  {
    HAL_Delay(2U);
    if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
    {
      return HCSR04_ECHO_STUCK_HIGH;
    }
  }

  return HCSR04_OK;
}

HCSR04_Result HCSR04_Measure(void)
{
  HCSR04_Result result = {HCSR04_TIMER_ERROR, 0U, 0U};

  if (timer_ready == 0U)
  {
    return result;
  }

  if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
  {
    HAL_Delay(2U);
    if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
    {
      result.status = HCSR04_ECHO_STUCK_HIGH;
      return result;
    }
  }

  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC1 | TIM_FLAG_CC2);

  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
  HCSR04_DelayUs(2U);
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
  HCSR04_DelayUs(HCSR04_TRIGGER_PULSE_US);
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

  if (HCSR04_WaitForCapture(TIM_FLAG_CC1, HCSR04_CAPTURE_TIMEOUT_MS) == 0U)
  {
    result.status = HCSR04_TIMEOUT;
    return result;
  }

  if (HCSR04_WaitForCapture(TIM_FLAG_CC2, HCSR04_CAPTURE_TIMEOUT_MS) == 0U)
  {
    result.status = HCSR04_TIMEOUT;
    return result;
  }

  result.echo_time_us = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_2);
  result.distance_mm =
      (uint32_t)(((uint64_t)result.echo_time_us * 343U + 1000U) / 2000U);

  if ((result.distance_mm < HCSR04_MIN_DISTANCE_MM) ||
      (result.distance_mm > HCSR04_MAX_DISTANCE_MM))
  {
    result.status = HCSR04_OUT_OF_RANGE;
  }
  else
  {
    result.status = HCSR04_OK;
  }

  return result;
}
