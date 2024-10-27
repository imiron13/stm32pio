#pragma once

#include <led.h>
#include <button.h>

Led_t<GPIO_PORT_ID_C, 6, false> led1;
DummyLed_t led2;
Button_t<GPIO_PORT_ID_C, 13,false> button2;
Button_t<GPIO_PORT_ID_B, 8,false> button1;

#define BOARD_HAS_SD_CARD                   (0)
