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
    GpioPinInterface_t *m_gpio;
    bool m_is_inverted_polarity;
public:
    Led_t(GpioPinInterface_t *gpio, bool is_inverted_polarity);

    void on();
    void off();
};
