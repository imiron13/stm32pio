#include <led.h>
#include <gpio_pin.h>

Led_t::Led_t(GpioPinInterface_t *gpio, bool is_inverted_polarity)
    : m_gpio(gpio)
    , m_is_inverted_polarity(is_inverted_polarity)
{
}

void Led_t::on()
{
    int gpio_value = (m_is_inverted_polarity ? 0 : 1);
    m_gpio->write(gpio_value);
}

void Led_t::off()
{
    int gpio_value = (m_is_inverted_polarity ? 1 : 0);
    m_gpio->write(gpio_value);
}
