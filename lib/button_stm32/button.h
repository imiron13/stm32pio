#pragma once

#include <gpio_pin.h>

template<GpioPortId_t port_id, int pin_id, bool is_inverted_polarity>
class Button_t
{
    GpioPin_t<port_id, pin_id> button_pin;
    static bool is_pressed_prev;
    static bool is_pressed_cur;

    bool _is_pressed_raw()
    {
        if (port_id == GPIO_PORT_ID_DUMMY)
        {
            return false;
        }
        else
        {
            return button_pin.is_high() ^ is_inverted_polarity;
        }
    }

public:
    bool is_pressed()
    {
        return is_pressed_cur;
    }

    void update()
    {
        is_pressed_prev = is_pressed_cur;
        is_pressed_cur = _is_pressed_raw();
    }

    bool is_pressed_event()
    {
        return is_pressed_cur && !is_pressed_prev;
    }

    bool is_released_event()
    {
        return is_pressed_prev && !is_pressed_cur;
    }
};

template<GpioPortId_t port_id, int pin_id, bool is_inverted_polarity>
bool Button_t<port_id,pin_id, is_inverted_polarity>::is_pressed_prev = false;
template<GpioPortId_t port_id, int pin_id, bool is_inverted_polarity>
bool Button_t<port_id,pin_id, is_inverted_polarity>::is_pressed_cur = false;

typedef Button_t<GPIO_PORT_ID_DUMMY, 0, 0> DummyButton_t;
