#include <keypad_4x1.h>
#include <gpio_pin.h>

Keypad_4x1::Keypad_4x1(GpioPinInterface_t *key1, GpioPinInterface_t *key2, GpioPinInterface_t *key3, GpioPinInterface_t *key4, GpioPinInterface_t *common)
    : m_gpio_button1(key1)
    , m_gpio_button2(key2)
    , m_gpio_button3(key3)
    , m_gpio_button4(key4)
    , m_gpio_common(common)
    , m_button1(key1, true)
    , m_button2(key2, true)
    , m_button3(key3, true)
    , m_button4(key4, true)
{
}

void Keypad_4x1::init()
{
    if (m_gpio_common)
    {
        m_gpio_common->config_output();
        m_gpio_common->write_low();
    }

    m_gpio_button1->config_input(true);
    m_gpio_button2->config_input(true);
    m_gpio_button3->config_input(true);
    m_gpio_button4->config_input(true);
}