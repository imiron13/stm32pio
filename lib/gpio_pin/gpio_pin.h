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

class GpioPinInterface
{
public:
    void writeHigh()
    {
        write(true);
    }

    void writeLow()
    {
        write(false);
    }

    bool isHigh()
    {
        return read();
    }

    virtual void configOutput() = 0;
    virtual void configInput(GpioPull_t pull=PULL_NONE) = 0;

    virtual void write(bool is_high) = 0;
    virtual bool read() = 0;
};

template<typename GPIO_PIN_TEMPLATE>
class GpioPin : public GpioPinInterface
{
public:
    GpioPin();

    virtual void write(bool is_high)
    {
        GPIO_PIN_TEMPLATE::write(is_high);
    }

    virtual bool read()
    {
        return GPIO_PIN_TEMPLATE::read();
    }

    virtual void configOutput()
    {
        GPIO_PIN_TEMPLATE::configOutput();
    }

    virtual void configInput(GpioPull_t pull=PULL_NONE)
    {
        if (pull == PULL_NONE)
        {
            GPIO_PIN_TEMPLATE::disablePullUpPullDown();
        }
        else if (pull == PULL_UP)
        {
            GPIO_PIN_TEMPLATE::enablePullUp();
        }
        else // pull == PULL_DOWN
        {
            GPIO_PIN_TEMPLATE::enablePullDown();
        }
        GPIO_PIN_TEMPLATE::configInput();        
    }
};

template<typename GPIO_PIN_TEMPLATE>
GpioPin<GPIO_PIN_TEMPLATE>::GpioPin()
    : GpioPinInterface()
{
}

class DummyGpioPinAlwaysLow : public GpioPinInterface
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

    virtual void configOutput()
    {
    }

    virtual void configInput(GpioPull_t pull=PULL_NONE)
    {
    }
};

class DummyGpioPinAlwaysHigh_t : public GpioPinInterface
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

    virtual void configOutput()
    {
    }

    virtual void configInput(GpioPull_t pull=PULL_NONE)
    {
    }
};
