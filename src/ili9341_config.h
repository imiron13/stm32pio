#pragma once

#include <gpio_pin_stm32.h>
#include <dma_gpio_pwm_stm32.h>

using Ili9341_bus = GpioPortA0_8Bit;
using Ili9341_dcPin = GpioPinTemplate<GPIO_PORT_ID_A, 15>;
using Ili9341_wrPin = GpioPinTemplate<GPIO_PORT_ID_A, 8>;
using Ili9341_resetPin = GpioPinTemplate<GPIO_PORT_ID_C, 11>;
using Ili9341_csPin = GpioPinTemplate<GPIO_PORT_ID_B, 2>;
using Ili9341_dma = DmaGpioPwm_8bitParallelWithWriteStrobe<Ili9341_bus,Ili9341_bus,Ili9341_bus>;
const uint32_t Ili9341_wstrb_delay_clk = 4;

using Ili9341_lowLevelInterface = Ili9341_lowLevelInterface_8bitParallel<Ili9341_bus, Ili9341_dcPin, Ili9341_wrPin, Ili9341_resetPin, Ili9341_csPin, Ili9341_dma, Ili9341_wstrb_delay_clk>;
using Ili9341 = Ili9341_driver<Ili9341_lowLevelInterface>;

using Ili9341_Color = ColorConverter<ColorFormatRgb565>;
