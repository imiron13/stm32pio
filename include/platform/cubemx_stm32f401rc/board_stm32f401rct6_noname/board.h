#pragma once

#include <led.h>
#include <button.h>

Led_t<GPIO_PORT_ID_C, 13, true> led1;
DummyLed_t led2;
ButtonTemplate_t<GPIO_PORT_ID_A, 0, true> button1;
DummyButtonTemplate_t button2;

#define BOARD_SUPPORT_SD_CARD                   (0)
#define BOARD_SUPPORT_AUDIO                     (0)
