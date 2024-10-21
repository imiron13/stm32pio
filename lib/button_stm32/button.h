#pragma once

#include <gpio_pin.h>

template<GpioPortId_t port_id, int pin_id, bool is_inverted_polarity>
class Button_t
{
    GpioPin_t<port_id, pin_id> button_pin;
public:
    bool is_pressed()
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
};

typedef Button_t<GPIO_PORT_ID_DUMMY, 0, 0> DummyButton_t;
