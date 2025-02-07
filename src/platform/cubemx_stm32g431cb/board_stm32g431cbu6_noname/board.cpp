#include <gpio_pin.h>
#include <gpio_pin_stm32.h>
#include <led.h>
#include <button.h>

GpioPinPortC_t<6> led1_gpio;
GpioPinPortB_t<8> button1_gpio;
GpioPinPortC_t<13> button2_gpio;
DummyGpioPinAlwaysLow_t dummy_gpio;

Led_t led1(&led1_gpio, false);
Led_t led2(&led1_gpio, false);

Button_t button1(&button1_gpio, false);
Button_t button2(&button2_gpio, false);
