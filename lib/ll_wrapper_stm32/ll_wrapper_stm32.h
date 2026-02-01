/*!
 * @file hal_wrapper_stm32.h
 * @brief Wrapper for STM32 HAL interface for different families
 *
 * Dependencies: 
 * - STM32 LL for different families
 * - STM32 FAMILY macro (F1/F2/F3/F4/F7/L0/L1/L4/G4)
 */

#if F0
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_tim.h"
#elif F1
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_tim.h"
#elif F2
#include "stm32f2xx_ll_gpio.h"
#include "stm32f2xx_ll_tim.h"
#elif F3
#include "stm32f3xx_ll_gpio.h"
#include "stm32f3xx_ll_tim.h"
#elif F4
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"
#elif F7
#include "stm32f7xx_ll_gpio.h"
#include "stm32f7xx_ll_tim.h"
#elif L0
#include "stm32l0xx_ll_gpio.h"
#include "stm32l0xx_ll_tim.h"
#elif L1
#include "stm32l1xx_ll_gpio.h"
#include "stm32l1xx_ll_tim.h"
#elif L4
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_tim.h"
#elif G0
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_tim.h"
#elif G4
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"
#else
#error "Unsupported STM32 Family"
#endif
