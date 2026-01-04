/*!
 * @file gpio_pin.h
 * @brief GPIO pin abstract logic
 *
 * Dependencies: none
 */

#pragma once

enum GpioPull_t
{
    PULL_NONE,
    PULL_UP,
    PULL_DOWN
};

class GpioPinInterface_t
{
public:
    void write_high()
    {
        write(true);
    }

    void write_low()
    {
        write(false);
    }

    bool is_high()
    {
        return read();
    }

    virtual void config_output() = 0;
    virtual void config_input(GpioPull_t pull=PULL_NONE) = 0;

    virtual void write(bool is_high) = 0;
    virtual bool read() = 0;
};

template<typename GPIO_PIN_TEMPLATE>
class GpioPin_t : public GpioPinInterface_t
{
public:
    GpioPin_t();

    virtual void write(bool is_high)
    {
        GPIO_PIN_TEMPLATE::write(is_high);
    }

    virtual bool read()
    {
        return GPIO_PIN_TEMPLATE::read();
    }

    virtual void config_output()
    {
        GPIO_PIN_TEMPLATE::config_output();
    }

    virtual void config_input(GpioPull_t pull=PULL_NONE)
    {
        if (pull == PULL_NONE)
        {
            GPIO_PIN_TEMPLATE::disable_pullup_pulldown();
        }
        else if (pull == PULL_UP)
        {
            GPIO_PIN_TEMPLATE::enable_pullup();
        }
        else // pull == PULL_DOWN
        {
            GPIO_PIN_TEMPLATE::enable_pulldown();
        }
        GPIO_PIN_TEMPLATE::config_input();        
    }
};

template<typename GPIO_PIN_TEMPLATE>
GpioPin_t<GPIO_PIN_TEMPLATE>::GpioPin_t()
    : GpioPinInterface_t()
{
}

class DummyGpioPinAlwaysLow : public GpioPinInterface_t
{
public:
    DummyGpioPinAlwaysLow() {}

    virtual void write(bool is_high)
    {
    }

    virtual bool read()
    {
        return false;
    }

    virtual void config_output()
    {
    }

    virtual void config_input(GpioPull_t pull=PULL_NONE)
    {
    }
};

class DummyGpioPinAlwaysHigh_t : public GpioPinInterface_t
{
public:
    DummyGpioPinAlwaysHigh_t() {}

    virtual void write(bool is_high)
    {
    }

    virtual bool read()
    {
        return true;
    }

    virtual void config_output()
    {
    }

    virtual void config_input(GpioPull_t pull=PULL_NONE)
    {
    }
};
