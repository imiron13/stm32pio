#pragma once

#include <led.h>
#include <button.h>

Led_t<GPIO_PORT_ID_C, 13, true> led1;
DummyLed_t led2;
Button_t<GPIO_PORT_ID_A, 0, true> button1;
DummyButton_t button2;

#define BOARD_HAS_SD_CARD                   (0)