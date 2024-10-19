#if F0
#include "stm32f0xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F0"
#elif F1
#include "stm32f1xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F1"
#elif F2
#include "stm32f2xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F2"
#elif F3
#include "stm32f3xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F3"
#elif F4
#include "stm32f4xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F4"
#elif F7
#include "stm32f7xx_hal.h"
#define STM32_FAMILY_NAME_STR       "F7"
#elif L0
#include "stm32l0xx_hal.h"
#define STM32_FAMILY_NAME_STR       "L0"
#elif L1
#include "stm32l1xx_hal.h"
#define STM32_FAMILY_NAME_STR       "L1"
#elif L4
#include "stm32l4xx_hal.h"
#define STM32_FAMILY_NAME_STR       "L4"
#else
#error "Unsupported STM32 Family"
#endif

#ifndef MCU_NAME_STR
#define MCU_NAME_STR    "STM32" STM32_FAMILY_NAME_STR "_XXX"
#endif
