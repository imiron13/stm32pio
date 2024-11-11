/*!
 * @file button.h
 * @brief Button driver
 *
 * Dependencies: 
 * - gpio_pin
 */

#pragma once

#include <gpio_pin.h>

class Button_t
{
public:
    Button_t(GpioPinInterface_t *gpio_pin, bool is_inverted_polarity=false);

    bool is_pressed();
    void update();
    bool is_pressed_event();
    bool is_released_event();

private:
    GpioPinInterface_t *m_gpio_pin;
    bool m_is_inverted_polarity;
    bool is_pressed_prev;
    bool is_pressed_cur;

    bool _is_pressed_raw();
};

class DummyButton_t : public Button_t
{
public:
    DummyGpioPinAlwaysLow_t m_dummy_gpio;

    DummyButton_t();
};
