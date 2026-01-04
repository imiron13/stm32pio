/* vim: set ai et ts=4 sw=4: */
#ifndef __ILI9341_H__
#define __ILI9341_H__

#include "fonts.h"
#include <stdbool.h>
#include <gpio_pin_stm32.h>

#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_RGB 0x00
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

/*** Redefine if necessary ***/

// default orientation
#define ILI9341_WIDTH  240
#define ILI9341_HEIGHT 320
//#define ILI9341_ROTATION (ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR)
#define ILI9341_ROTATION (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)

// rotate right
/*
#define ILI9341_WIDTH  320
#define ILI9341_HEIGHT 240
#define ILI9341_ROTATION (ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
*/

// rotate left
/*
#define ILI9341_WIDTH  320
#define ILI9341_HEIGHT 240
#define ILI9341_ROTATION (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
*/

// upside down
/*
#define ILI9341_WIDTH  240
#define ILI9341_HEIGHT 320
#define ILI9341_ROTATION (ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR)
*/

/****************************/

// Color definitions
#define	ILI9341_BLACK   0x0000
#define	ILI9341_BLUE    0x001F
#define	ILI9341_RED     0xF800
#define	ILI9341_GREEN   0x07E0
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_WHITE   0xFFFF
#define ILI9341_COLOR565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

template<class BUS, class GPIO_DC, class GPIO_WR, class GPIO_RESET, class GPIO_CS, uint32_t WSTRB_DELAY>
class Ili9341_lowLevelInterface_8bitParallel
{
public:
    static void select() {
        GPIO_CS::write_low();
    }

    static void unselect() {
        GPIO_CS::write_high();
    }

    static void dataMode() {
        GPIO_DC::write_high();
    }

    static void commandMode() {
        GPIO_DC::write_low();
    }

    static void writeStobe() {
        GPIO_WR::write_low();
        for (uint32_t i = 0; i < WSTRB_DELAY; i++) __NOP();
        GPIO_WR::write_high();
    }

    static void setDataBus(uint8_t data) {
        BUS::write(data);
    }

    static void writeCommand(uint8_t cmd, bool doSelect=true) {
        if (doSelect) select();
        commandMode();
        setDataBus(cmd);
        writeStobe();
        if (doSelect) unselect();
    }

    static void writeData(uint8_t* buff, size_t buff_size, bool doSelect=true)
    {
        if (doSelect) select();
        dataMode();
        for (size_t i = 0; i < buff_size; i++) {
            setDataBus(buff[i]);
            writeStobe();
        }
        if (doSelect) unselect();
    }

    static void reset(uint32_t delay_ms) {
        GPIO_RESET::write_low();
        HAL_Delay(delay_ms);
        GPIO_RESET::write_high();
    }

    static void dmaMode() {
        GPIO_WR::config_alternate_function();
    }

    static void controlMode() {
        GPIO_WR::write_high();
        GPIO_WR::config_output();
    }
};


template<class LL_IF>
class Ili9341_driver
{
public:
    static void init();
    static void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    static void fillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    static void fillScreen(uint16_t color);
    static void drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
    static void invertColors(bool invert);
    static void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    static void setXWindow(uint16_t x0, uint16_t x1);
    static void setYWindow(uint16_t y0, uint16_t y1);
    static void enableRamAccess();
    static void writeString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);

    static void dmaMode();
    static void controlMode();
    static void restartCs(uint32_t pulse_width_clk);
};

template<class LL_IF>
void Ili9341_driver<LL_IF>::dmaMode()
{
    LL_IF::dataMode();
    LL_IF::dmaMode();
    LL_IF::select();
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::controlMode()
{
    LL_IF::controlMode();
    LL_IF::select();
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::restartCs(uint32_t pulse_width_clk)
{
    LL_IF::unselect();
    for (uint32_t i = 0; i < pulse_width_clk; i++) __NOP();
    LL_IF::select();
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::init()
{
    const uint32_t RESET_PULSE_MS = 5;
    LL_IF::controlMode();
    LL_IF::select();
    LL_IF::reset(RESET_PULSE_MS);

    // command list is based on https://github.com/martnak/STM32-ILI9341

    // SOFTWARE RESET
    LL_IF::writeCommand(0x01);
    HAL_Delay(1000);
        
    // POWER CONTROL A
    LL_IF::writeCommand(0xCB);
    {
        uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        LL_IF::writeData(data, sizeof(data));
    }

    // POWER CONTROL B
    LL_IF::writeCommand(0xCF);
    {
        uint8_t data[] = { 0x00, 0xC1, 0x30 };
        LL_IF::writeData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL A
    LL_IF::writeCommand(0xE8);
    {
        uint8_t data[] = { 0x85, 0x00, 0x78 };
        LL_IF::writeData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL B
    LL_IF::writeCommand(0xEA);
    {
        uint8_t data[] = { 0x00, 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    // POWER ON SEQUENCE CONTROL
    LL_IF::writeCommand(0xED);
    {
        uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
        LL_IF::writeData(data, sizeof(data));
    }

    // PUMP RATIO CONTROL
    LL_IF::writeCommand(0xF7);
    {
        uint8_t data[] = { 0x20 };
        LL_IF::writeData(data, sizeof(data));
    }

    // POWER CONTROL,VRH[5:0]
    LL_IF::writeCommand(0xC0);
    {
        uint8_t data[] = { 0x23 };
        LL_IF::writeData(data, sizeof(data));
    }

    // POWER CONTROL,SAP[2:0];BT[3:0]
    LL_IF::writeCommand(0xC1);
    {
        uint8_t data[] = { 0x10 };
        LL_IF::writeData(data, sizeof(data));
    }

    // VCM CONTROL
    LL_IF::writeCommand(0xC5);
    {
        uint8_t data[] = { 0x3E, 0x28 };
        LL_IF::writeData(data, sizeof(data));
    }

    // VCM CONTROL 2
    LL_IF::writeCommand(0xC7);
    {
        uint8_t data[] = { 0x86 };
        LL_IF::writeData(data, sizeof(data));
    }

    // MEMORY ACCESS CONTROL
    LL_IF::writeCommand(0x36);
    {
        uint8_t data[] = { 0x48 };
        LL_IF::writeData(data, sizeof(data));
    }

    // PIXEL FORMAT
    LL_IF::writeCommand(0x3A);
    {
        uint8_t data[] = { 0x55 };
        LL_IF::writeData(data, sizeof(data));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    LL_IF::writeCommand(0xB1);
    {
        uint8_t data[] = { 0x00, 0x18 };
        LL_IF::writeData(data, sizeof(data));
    }

    // DISPLAY FUNCTION CONTROL
    LL_IF::writeCommand(0xB6);
    {
        uint8_t data[] = { 0x08, 0x82, 0x27 };
        LL_IF::writeData(data, sizeof(data));
    }

    // 3GAMMA FUNCTION DISABLE
    LL_IF::writeCommand(0xF2);
    {
        uint8_t data[] = { 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    // GAMMA CURVE SELECTED
    LL_IF::writeCommand(0x26);
    {
        uint8_t data[] = { 0x01 };
        LL_IF::writeData(data, sizeof(data));
    }

    // POSITIVE GAMMA CORRECTION
    LL_IF::writeCommand(0xE0);
    {
        uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                           0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    // NEGATIVE GAMMA CORRECTION
    LL_IF::writeCommand(0xE1);
    {
        uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                           0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
        LL_IF::writeData(data, sizeof(data));
    }

    // EXIT SLEEP
    LL_IF::writeCommand(0x11);
    HAL_Delay(120);

    // TURN ON DISPLAY
    LL_IF::writeCommand(0x29);
    // MADCTL
    LL_IF::writeCommand(0x36);
    {
        uint8_t data[] = { ILI9341_ROTATION };
        LL_IF::writeData(data, sizeof(data));
    }

    LL_IF::unselect();    
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    setXWindow(x0, x1);
    setYWindow(y0, y1);
    enableRamAccess();
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::setXWindow(uint16_t x0, uint16_t x1)
{
    // column address set
    LL_IF::writeCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        LL_IF::writeData(data, sizeof(data));
    }
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::setYWindow(uint16_t y0, uint16_t y1)
{
    // row address set
    LL_IF::writeCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        LL_IF::writeData(data, sizeof(data));
    }
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::enableRamAccess()
{
    // write to RAM
    LL_IF::writeCommand(0x2C); // RAMWR
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{

}

template<class LL_IF>
void Ili9341_driver<LL_IF>::fillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::fillScreen(uint16_t color)
{
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data)
{
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::invertColors(bool invert)
{
}

template<class LL_IF>
void Ili9341_driver<LL_IF>::writeString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor)
{
}

#include "ili9341_config.h"

#endif // __ILI9341_H__
