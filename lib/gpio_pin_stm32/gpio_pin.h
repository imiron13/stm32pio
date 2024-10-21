#pragma once

#include <stdint.h>
#include <hal_wrapper_stm32.h>

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
class GpioPin_t
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
