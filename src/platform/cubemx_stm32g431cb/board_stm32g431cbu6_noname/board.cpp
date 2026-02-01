#include <gpio_pin.h>
#include <gpio_pin_stm32.h>
#include <led.h>
#include <button.h>

GpioPinStm32 led1_gpio(GPIOC, 6);
GpioPinStm32 button1_gpio(GPIOB, 8);
GpioPinStm32 button2_gpio(GPIOC, 13);
DummyGpioPinAlwaysLow dummy_gpio;

Led_t led1(&led1_gpio, false);
Led_t led2(&led1_gpio, false);

Button_t button1(&button1_gpio, false);
Button_t button2(&button2_gpio, false);
