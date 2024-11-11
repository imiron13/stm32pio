#include <button.h>

Button_t::Button_t(GpioPinInterface_t *gpio_pin, bool is_inverted_polarity)
    : m_gpio_pin(gpio_pin)
    , m_is_inverted_polarity(is_inverted_polarity)
{

}

bool Button_t::_is_pressed_raw()
{
    return m_gpio_pin->is_high() ^ m_is_inverted_polarity;
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
