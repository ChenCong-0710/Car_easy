#include "hcsr04.h"

#define HCSR04_TRIGGER_PULSE_US       15U
#define HCSR04_ECHO_START_TIMEOUT_US  30000U
#define HCSR04_ECHO_HIGH_TIMEOUT_US   30000U
#define HCSR04_MIN_DISTANCE_MM        20U
#define HCSR04_MAX_DISTANCE_MM        4000U

static uint32_t cycles_per_us;
static uint8_t cycle_counter_ready;

static uint8_t HCSR04_CycleCounterInit(void)
{
  uint32_t start;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
  start = DWT->CYCCNT;
  __NOP();
  __NOP();
  __NOP();

  return ((cycles_per_us != 0U) && (DWT->CYCCNT != start)) ? 1U : 0U;
}

static void HCSR04_DelayUs(uint32_t delay_us)
{
  const uint32_t start = DWT->CYCCNT;
  const uint32_t delay_cycles = delay_us * cycles_per_us;

  while ((uint32_t)(DWT->CYCCNT - start) < delay_cycles)
  {
  }
}

static uint8_t HCSR04_WaitWhile(GPIO_PinState state, uint32_t timeout_us)
{
  const uint32_t start = DWT->CYCCNT;
  const uint32_t timeout_cycles = timeout_us * cycles_per_us;

  while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == state)
  {
    if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles)
    {
      return 0U;
    }
  }

  return 1U;
}

void HCSR04_Init(void)
{
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
  cycle_counter_ready = HCSR04_CycleCounterInit();
}

HCSR04_Status HCSR04_SelfTest(void)
{
  if (cycle_counter_ready == 0U)
  {
    return HCSR04_TIMER_ERROR;
  }

  if ((HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET) &&
      (HCSR04_WaitWhile(GPIO_PIN_SET, 2000U) == 0U))
  {
    return HCSR04_ECHO_STUCK_HIGH;
  }

  return HCSR04_OK;
}

HCSR04_Result HCSR04_Measure(void)
{
  HCSR04_Result result = {HCSR04_TIMER_ERROR, 0U, 0U};
  uint32_t echo_start;

  if (cycle_counter_ready == 0U)
  {
    return result;
  }

  if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
  {
    if (HCSR04_WaitWhile(GPIO_PIN_SET, 2000U) == 0U)
    {
      result.status = HCSR04_ECHO_STUCK_HIGH;
      return result;
    }
  }

  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
  HCSR04_DelayUs(2U);
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
  HCSR04_DelayUs(HCSR04_TRIGGER_PULSE_US);
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

  if (HCSR04_WaitWhile(GPIO_PIN_RESET, HCSR04_ECHO_START_TIMEOUT_US) == 0U)
  {
    result.status = HCSR04_TIMEOUT;
    return result;
  }

  echo_start = DWT->CYCCNT;
  if (HCSR04_WaitWhile(GPIO_PIN_SET, HCSR04_ECHO_HIGH_TIMEOUT_US) == 0U)
  {
    result.status = HCSR04_TIMEOUT;
    return result;
  }

  result.echo_time_us = (uint32_t)(DWT->CYCCNT - echo_start) / cycles_per_us;
  result.distance_mm = (uint32_t)(((uint64_t)result.echo_time_us * 343U + 1000U) / 2000U);

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
