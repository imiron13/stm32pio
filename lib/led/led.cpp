#include <led.h>
#include <gpio_pin.h>

Led_t::Led_t(GpioPinInterface_t *gpio, bool is_active_low)
    : m_gpio(gpio)
    , m_is_active_low(is_active_low)
{
}

void Led_t::init()
{
    m_gpio->config_output();
}

void Led_t::on()
{
    int gpio_value = (m_is_active_low ? 0 : 1);
    m_gpio->write(gpio_value);
}

void Led_t::off()
{
    int gpio_value = (m_is_active_low ? 1 : 0);
    m_gpio->write(gpio_value);
}
