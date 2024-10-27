#pragma once

#include <led.h>
#include <button.h>

Led_t<GPIO_PORT_ID_B, 9, true> led1;
Led_t<GPIO_PORT_ID_E, 5, true> led2;
Button_t<GPIO_PORT_ID_E, 4,true> button1;   // button label: KO
Button_t<GPIO_PORT_ID_A, 0,false> button2;  // button label: WAKE_UP

#define BOARD_HAS_SD_CARD                   (1)
