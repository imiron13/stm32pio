#include <gpio_pin.h>
#include <button.h>

#pragma once

// Pinout left to right: ground, key2, key1, key4, key3
class Keypad_4x1 
{
    GpioPinInterface_t *m_gpio_button1;
    GpioPinInterface_t *m_gpio_button2;
    GpioPinInterface_t *m_gpio_button3;
    GpioPinInterface_t *m_gpio_button4;
    GpioPinInterface_t *m_gpio_common;
    
    Button_t m_button1;
    Button_t m_button2;
    Button_t m_button3;
    Button_t m_button4;

public:
    Keypad_4x1(
        GpioPinInterface_t *key1, GpioPinInterface_t *key2, GpioPinInterface_t *key3, 
        GpioPinInterface_t *key4, GpioPinInterface_t *common=nullptr
    );

    void init();
    void update();

    Button_t &button1() { return m_button1; }
    Button_t &button2() { return m_button2; }
    Button_t &button3() { return m_button3; }
    Button_t &button4() { return m_button4; }
};
