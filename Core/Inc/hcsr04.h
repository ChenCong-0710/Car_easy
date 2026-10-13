#ifndef __HCSR04_H__
#define __HCSR04_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
  HCSR04_OK = 0,
  HCSR04_TIMEOUT,
  HCSR04_OUT_OF_RANGE,
  HCSR04_ECHO_STUCK_HIGH,
  HCSR04_TIMER_ERROR
} HCSR04_Status;

typedef struct
{
  HCSR04_Status status;
  uint32_t echo_time_us;
  uint32_t distance_mm;
} HCSR04_Result;

void HCSR04_Init(void);
HCSR04_Status HCSR04_SelfTest(void);
HCSR04_Result HCSR04_Measure(void);

#ifdef __cplusplus
}
#endif

#endif /* __HCSR04_H__ */
