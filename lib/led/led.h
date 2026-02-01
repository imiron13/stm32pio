/*!
 * @file led.h
 * @brief Led driver
 *
 * Dependencies: 
 * - gpio_pin
 */

#pragma once

#include <gpio_pin.h>

class Led_t
{
    GpioPinInterface *m_gpio;
    bool m_is_active_low;
public:
    Led_t(GpioPinInterface *gpio, bool is_active_low=false);

    void init();
    void on();
    void off();
};
