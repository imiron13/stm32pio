#include <button.h>

Button_t::Button_t(GpioPinInterface_t *gpio_pin, bool is_active_low)
    : m_gpio_pin(gpio_pin)
    , m_is_active_low(is_active_low)
    , is_pressed_prev(false)
    , is_pressed_cur(false)
{

}

void Button_t::init(bool need_pull)
{
    GpioPull_t pull = PULL_NONE;
    if (need_pull)
    {
        pull = (m_is_active_low ? PULL_UP : PULL_DOWN);
    }
    m_gpio_pin->config_input(pull);
    is_pressed_prev = _is_pressed_raw();
    is_pressed_cur = is_pressed_prev;
}

bool Button_t::_is_pressed_raw()
{
    return m_gpio_pin->is_high() ^ m_is_active_low;
}

bool Button_t::is_pressed()
{
    return is_pressed_cur;
}

void Button_t::update()
{
    is_pressed_prev = is_pressed_cur;
    is_pressed_cur = _is_pressed_raw();
}

bool Button_t::is_pressed_event()
{
    return is_pressed_cur && !is_pressed_prev;
}

bool Button_t::is_released_event()
{
    return is_pressed_prev && !is_pressed_cur;
}

DummyButton_t::DummyButton_t()
    : Button_t(&m_dummy_gpio)
{

}
