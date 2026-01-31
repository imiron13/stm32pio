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
    GPIO_PORT_ID_F,
    GPIO_PORT_ID_G,
    GPIO_PORT_ID_DUMMY = INT32_MAX
};

template<GpioPortId_t port_id, int pin_id>
class GpioPinTemplate
{
    constexpr static GPIO_TypeDef *const gpio[7] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };
public:
    static void write_high()
    {
        gpio[port_id]->BSRR = (1U << pin_id);
    }

    static void write_low()
    {
        gpio[port_id]->BRR = (1U << pin_id);
    }

    static void write(bool is_high)
    {
        if (is_high)
        {
            write_high();
        }
        else
        {
            write_low();
        }
    }

    static bool read()
    {
        return (gpio[port_id]->IDR & (1U << pin_id)) != 0;
    }

    static bool is_high()
    {
        return read();
    }

    static void set_mode(uint32_t mode)
    {
        gpio[port_id]->MODER &= ~(0x3U << (pin_id * 2));
        gpio[port_id]->MODER |=  (mode << (pin_id * 2));
    }

    static void config_output()
    {
        set_mode(0x1U); // General purpose output mode
    }

    static void config_input()
    {
        set_mode(0x0U); // Input mode
    }

    static void config_alternate_function()
    {
        set_mode(0x2U); // Alternate function mode
    }

    static void enable_pullup()
    {
        gpio[port_id]->PUPDR &= ~(0x3U << (pin_id * 2));
        gpio[port_id]->PUPDR |=  (0x1U << (pin_id * 2));
    }

    static void enable_pulldown()
    {
        gpio[port_id]->PUPDR &= ~(0x3U << (pin_id * 2));
        gpio[port_id]->PUPDR |=  (0x2U << (pin_id * 2));
    }

    static void disable_pullup_pulldown()
    {
        gpio[port_id]->PUPDR &= ~(0x3U << (pin_id * 2));
    }
};

//template<int PIN_ID>
//typedef GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA;
//typedef GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA

template <size_t PIN_ID>
using GpioPinPortA = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortB = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_B, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortC = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_C, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortD = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_D, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortE = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_E, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortF = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_F, PIN_ID>>;

template <size_t PIN_ID>
using GpioPinPortG = GpioPin_t<GpioPinTemplate<GPIO_PORT_ID_G, PIN_ID>>;

template<GpioPortId_t port_id, int byte_index>
class GpioPort8BitTemplate
{
    constexpr static GPIO_TypeDef *const gpio[7] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };
public:
    static void set_mode(uint32_t mode)
    {
        gpio[port_id]->MODER &= ~(0xFFFFU << (byte_index * 16));
        gpio[port_id]->MODER |= (mode * 0x5555U << (byte_index * 16));
    }

    static void config_output()
    {
        set_mode(0x1U); // General purpose output mode
    }

    static void config_input()
    {
        set_mode(0x0U); // Input mode
    }

    static void write(uint8_t data)
    {
        *((uint8_t*)&gpio[port_id]->ODR + byte_index) = data;
    }
};

template<GpioPortId_t port_id>
class GpioPort16BitTemplate
{
    constexpr static GPIO_TypeDef *const gpio[7] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };
public:
    static void set_mode(uint32_t mode)
    {
        gpio[port_id]->MODER = mode * 0x55555555U;
    }

    static void config_output()
    {
        set_mode(0x1U); // General purpose output mode
    }

    static void config_input()
    {
        set_mode(0x0U); // Input mode
    }

    static void write(uint16_t data)
    {
        *(uint16_t*)&gpio[port_id]->ODR = data;
    }
};

using GpioPortA0_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_A, 0>;
using GpioPortA8_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_A, 1>;
using GpioPortA_16Bit = GpioPort16BitTemplate<GPIO_PORT_ID_A>;
using GpioPortB0_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_B, 0>;
using GpioPortB8_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_B, 1>;
using GpioPortB_16Bit = GpioPort16BitTemplate<GPIO_PORT_ID_B>;
