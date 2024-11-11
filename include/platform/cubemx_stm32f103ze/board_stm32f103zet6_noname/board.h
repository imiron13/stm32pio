#pragma once

#include <led.h>
#include <button.h>
#include <gpio_pin_stm32.h>

GpioPinPortB_t<9> led1_gpio;
GpioPinPortE_t<5> led2_gpio;
GpioPinPortE_t<4> button1_gpio;
GpioPinPortA_t<0> button2_gpio;

Led_t led1(&led1_gpio, true);
Led_t led2(&led2_gpio, true);

Button_t button1(&button1_gpio, true);   // button label: KO
Button_t button2(&button2_gpio, false);  // button label: WAKE_UP

#define BOARD_SUPPORT_SD_CARD                   (1)
#define BOARD_SUPPORT_AUDIO                     (1)
