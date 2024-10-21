#pragma once

#include <gpio_pin.h>

template<GpioPortId_t port_id, int pin_id, bool is_inverted_polarity>
class Led_t
{
    GpioPin_t<port_id, pin_id> led_pin;
public:
    void on()
    {
        if (port_id != GPIO_PORT_ID_DUMMY)
        {
            int gpio_value = (is_inverted_polarity ? 0 : 1);
            led_pin.write(gpio_value);
        }
    }

    void off()
    {
        if (port_id != GPIO_PORT_ID_DUMMY)
        {
            int gpio_value = (is_inverted_polarity ? 1 : 0);
            led_pin.write(gpio_value);
        }
    }
};

typedef Led_t<GPIO_PORT_ID_DUMMY, 0, false> DummyLed_t;
