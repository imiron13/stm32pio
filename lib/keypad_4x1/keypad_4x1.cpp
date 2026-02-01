#include <keypad_4x1.h>
#include <gpio_pin.h>

Keypad_4x1::Keypad_4x1(
    GpioPinInterface *key1, GpioPinInterface *key2, GpioPinInterface *key3, 
    GpioPinInterface *key4, GpioPinInterface *common
)
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
        m_gpio_common->configOutput();
        m_gpio_common->writeLow();
    }

    m_button1.init(true);
    m_button2.init(true);
    m_button3.init(true);
    m_button4.init(true);}

void Keypad_4x1::update()
{
    m_button1.update();
    m_button2.update();
    m_button3.update();
    m_button4.update();
}
