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
#include <ll_wrapper_stm32.h>
#include <gpio_pin.h>

enum GpioPortId
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

constexpr static GPIO_TypeDef *const GpioPort_regsLut[7] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };

inline void GpioPin_setMode(GPIO_TypeDef* regs, uint32_t pin_id, uint32_t mode)
{
    uint32_t bit_shift = pin_id * 2;
    regs->MODER = (regs->MODER & ~(0x3U << bit_shift)) | (mode << bit_shift);
}

inline void GpioPin_configPull(GPIO_TypeDef* regs, uint32_t pin_id, uint32_t pull_config)
{
    uint32_t bit_shift = pin_id * 2;
    regs->PUPDR = (regs->PUPDR & ~(0x3U << bit_shift)) | (pull_config << bit_shift);
}

template<GpioPortId port_id, int pin_id>
class GpioPinTemplate
{
public:
    static void writeHigh()
    {
        LL_GPIO_SetOutputPin(GpioPort_regsLut[port_id], 1U << pin_id);
    }

    static void writeLow()
    {
        LL_GPIO_ResetOutputPin(GpioPort_regsLut[port_id], 1U << pin_id);
    }

    static void write(bool is_high)
    {
        if (is_high)
        {
            writeHigh();
        }
        else
        {
            writeLow();
        }
    }

    static bool read()
    {
        return LL_GPIO_IsInputPinSet(GpioPort_regsLut[port_id], 1U << pin_id);
    }

    static bool isHigh()
    {
        return read();
    }

    static void setMode(uint32_t mode)
    {
        GpioPin_setMode(GpioPort_regsLut[port_id], pin_id, mode);
    }

    static void configOutput()
    {
        setMode(LL_GPIO_MODE_OUTPUT);
    }

    static void configInput()
    {
        setMode(LL_GPIO_MODE_INPUT);
    }

    static void configAlternateFunction()
    {
        setMode(LL_GPIO_MODE_ALTERNATE);
    }

    static void configPull(uint32_t pull_config)
    {
        GpioPin_configPull(GpioPort_regsLut[port_id], pin_id, pull_config);
    }

    static void enablePullUp()
    {
        configPull(LL_GPIO_PULL_UP);
    }

    static void enablePullDown()
    {
        configPull(LL_GPIO_PULL_DOWN);
    }

    static void disablePullUpPullDown()
    {
        configPull(LL_GPIO_PULL_NO);
    }
};

class GpioPinStm32 : public GpioPinInterface
{
    GPIO_TypeDef* m_gpioRegs;
    uint32_t m_pinMask;

public:
    GpioPinStm32(GPIO_TypeDef* gpioRegs, uint32_t pinId)
        : m_gpioRegs(gpioRegs)
        , m_pinMask(1U << pinId)
    {}

    virtual void write(bool is_high)
    {
        if (is_high)
        {
            LL_GPIO_SetOutputPin(m_gpioRegs, m_pinMask);
        }
        else
        {
            LL_GPIO_ResetOutputPin(m_gpioRegs, m_pinMask);
        }
    }

    virtual bool read()
    {
        return LL_GPIO_IsInputPinSet(m_gpioRegs, m_pinMask);
    }

    virtual void configOutput()
    {
        LL_GPIO_SetPinMode(m_gpioRegs, m_pinMask, LL_GPIO_MODE_OUTPUT);
    }

    virtual void configInput(GpioPull_t pull=PULL_NONE)
    {
        LL_GPIO_SetPinMode(m_gpioRegs, m_pinMask, LL_GPIO_MODE_INPUT);
        if (pull == PULL_NONE)
        {
            LL_GPIO_SetPinPull(m_gpioRegs, m_pinMask, LL_GPIO_PULL_NO);
        }
        else if (pull == PULL_UP)
        {
            LL_GPIO_SetPinPull(m_gpioRegs, m_pinMask, LL_GPIO_PULL_UP);
        }
        else if (pull == PULL_DOWN)
        {
            LL_GPIO_SetPinPull(m_gpioRegs, m_pinMask, LL_GPIO_PULL_DOWN);
        }
    }
};

//template<int PIN_ID>
//typedef GpioPin<GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA;
//typedef GpioPin<GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>> GpioPinPortA

template <size_t PIN_ID>
using GpioPinA = GpioPinTemplate<GPIO_PORT_ID_A, PIN_ID>;

template <size_t PIN_ID>
using GpioPinB = GpioPinTemplate<GPIO_PORT_ID_B, PIN_ID>;

template <size_t PIN_ID>
using GpioPinC = GpioPinTemplate<GPIO_PORT_ID_C, PIN_ID>;

template <size_t PIN_ID>
using GpioPinD = GpioPinTemplate<GPIO_PORT_ID_D, PIN_ID>;

template <size_t PIN_ID>
using GpioPinE = GpioPinTemplate<GPIO_PORT_ID_E, PIN_ID>;

template <size_t PIN_ID>
using GpioPinF = GpioPinTemplate<GPIO_PORT_ID_F, PIN_ID>;

template <size_t PIN_ID>
using GpioPinG = GpioPinTemplate<GPIO_PORT_ID_G, PIN_ID>;

template<GpioPortId port_id, int byte_index>
class GpioPort8BitTemplate
{
public:
    static void setMode(uint32_t mode)
    {
        GpioPort_regsLut[port_id]->MODER = (GpioPort_regsLut[port_id]->MODER & ~(0xFFFFU << (byte_index * 16))) | (mode * 0x5555U << (byte_index * 16));
    }

    static void configOutput()
    {
        setMode(LL_GPIO_MODE_OUTPUT);
    }

    static void configInput()
    {
        setMode(LL_GPIO_MODE_INPUT);
    }

    static void write(uint8_t data)
    {
        *((uint8_t*)&GpioPort_regsLut[port_id]->ODR + byte_index) = data;
    }
};

template<GpioPortId port_id>
class GpioPort16BitTemplate
{
public:
    static void setMode(uint32_t mode)
    {
        GpioPort_regsLut[port_id]->MODER = mode * 0x55555555U;
    }

    static void configOutput()
    {
        setMode(LL_GPIO_MODE_OUTPUT);
    }

    static void configInput()
    {
        setMode(LL_GPIO_MODE_INPUT);
    }

    static void write(uint16_t data)
    {
        *(uint16_t*)&GpioPort_regsLut[port_id]->ODR = data;
    }
};

using GpioPortA0_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_A, 0>;
using GpioPortA8_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_A, 1>;
using GpioPortA_16Bit = GpioPort16BitTemplate<GPIO_PORT_ID_A>;
using GpioPortB0_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_B, 0>;
using GpioPortB8_8Bit = GpioPort8BitTemplate<GPIO_PORT_ID_B, 1>;
using GpioPortB_16Bit = GpioPort16BitTemplate<GPIO_PORT_ID_B>;
