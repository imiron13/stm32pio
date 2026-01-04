#include <gpio_pin.h>
#include <gpio_pin_stm32.h>
#include <led.h>
#include <button.h>

GpioPinPortC<6> led1_gpio;
GpioPinPortB<8> button1_gpio;
GpioPinPortC<13> button2_gpio;
DummyGpioPinAlwaysLow dummy_gpio;

Led_t led1(&led1_gpio, false);
Led_t led2(&led1_gpio, false);

Button_t button1(&button1_gpio, false);
Button_t button2(&button2_gpio, false);
