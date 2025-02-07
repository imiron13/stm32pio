#pragma once

#include <gpio_pin.h>
#include <led.h>
#include <button.h>

extern GpioPinPortC_t<6> led1_gpio;
extern GpioPinPortB_t<8> button1_gpio;
extern GpioPinPortC_t<13> button2_gpio;
extern DummyGpioPinAlwaysLow_t dummy_gpio;

extern Led_t led1;
extern Led_t led2;

extern Button_t button1;
extern Button_t button2;

#define BOARD_SUPPORT_SD_CARD                   (0)
#define BOARD_SUPPORT_AUDIO                     (0)
