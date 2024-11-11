/*!
 * @file gpio_pin_stm32.h
 * @brief GPIO pin logic for STM32
 *
 * Dependencies: 
 * - gpio_pin
 * - hal_wrapper_stm32
 */

#pragma once

#include <stdint.h>
#include <hal_wrapper_stm32.h>
#include <gpio_pin.h>

enum GpioPortId_t
{
    GPIO_PORT_ID_A,
    GPIO_PORT_ID_B,
    GPIO_PORT_ID_C,
    GPIO_PORT_ID_D,
    GPIO_PORT_ID_E,
    GPIO_PORT_ID_DUMMY = INT32_MAX
};

template<GpioPortId_t port_id, int pin_id>
class GpioPinTemplate_t
{
    constexpr static GPIO_TypeDef *const gpio[5] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE };
public:
    static void write_high()
    {
        write(true);
    }

    static void write_low()
    {
        write(false);
    }

    static void write(bool is_high)
    {
        GPIO_PinState pin_state = (is_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(gpio[port_id], 1U << pin_id, pin_state);
    }

    static bool read()
    {
        return HAL_GPIO_ReadPin(gpio[port_id], 1U << pin_id);
    }

    static bool is_high()
    {
        return read();
    }
};

//template<int PIN_ID>
//typedef GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA_t;
//typedef GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA_t

template <size_t PIN_ID>
using GpioPinPortA_t = GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_A, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortB_t = GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_B, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortC_t = GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_C, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortD_t = GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_D, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortE_t = GpioPin_t<GpioPinTemplate_t<GPIO_PORT_ID_E, PIN_ID>>;