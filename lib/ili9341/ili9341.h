#pragma once

#include "fonts.h"
#include <intrinsics.h>
#include <stdbool.h>
#include <gpio_pin_stm32.h>

//******************************************************************************
// Color format
//******************************************************************************
/* [15:11] - R, [10:5] - G, [4:0] - B */
class ColorFormatRgb565
{
public:
    static constexpr uint16_t get(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint16_t)( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3) );
    }
};

/* [15:11] - B, [10:5] - G, [4:0] - R */
class ColorFormatBgr565
{
public:
    static constexpr uint16_t get(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint16_t)( ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | ((r & 0xF8) >> 3) );
    }
};

template<class COLOR_FORMAT>
class ColorConverter
{
public:
    static constexpr uint16_t color(uint8_t r, uint8_t g, uint8_t b)
    {
        return COLOR_FORMAT::get(r, g, b);
    }

    constexpr static uint16_t BLACK = color(0, 0, 0);
    constexpr static uint16_t BLUE = color(0, 0, 255);
    constexpr static uint16_t RED = color(255, 0, 0);
    constexpr static uint16_t GREEN = color(0, 255, 0);
    constexpr static uint16_t CYAN = color(0, 255, 255);
    constexpr static uint16_t MAGENTA = color(255, 0, 255);
    constexpr static uint16_t YELLOW = color(255, 255, 0);
    constexpr static uint16_t WHITE = color(255, 255, 255);
};

//******************************************************************************
// Low-level interface
//******************************************************************************
template<class BUS, class GPIO_DC, class GPIO_WR, class GPIO_RESET, class GPIO_CS, class DMA, uint32_t WSTRB_DELAY>
class Ili9341_lowLevelInterface_8bitParallel
{
public:
    static void init() {
        DMA::init();

        GPIO_CS::writeHigh();
        GPIO_WR::writeHigh();
        GPIO_RESET::writeHigh();
        GPIO_DC::writeLow();
        BUS::write(0x00);

        GPIO_CS::configOutput();
        GPIO_DC::configOutput();
        GPIO_WR::configOutput();
        GPIO_RESET::configOutput();
        BUS::configOutput();
    }

    static void select() {
        GPIO_CS::writeLow();
    }

    static void unselect() {
        GPIO_CS::writeHigh();
    }

    static void dataMode() {
        GPIO_DC::writeHigh();
    }

    static void commandMode() {
        GPIO_DC::writeLow();
    }

    static void writeStobe() {
        GPIO_WR::writeLow();
        for (uint32_t i = 0; i < WSTRB_DELAY; i++) __NOP();
        GPIO_WR::writeHigh();
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
        GPIO_RESET::writeLow();
        HAL_Delay(delay_ms);
        GPIO_RESET::writeHigh();
    }

    static void dmaMode() {
        GPIO_WR::configAlternateFunction();
    }

    static void controlMode() {
        GPIO_WR::writeHigh();
        GPIO_WR::configOutput();
    }

    static void setupCircularDoubleBufferMode(uint8_t* buff, size_t buff_size) {
        DMA::setupCircularDoubleBufferMode(buff, buff_size);
    }

    static void startTransfer() {
        DMA::startTransfer();
    }

    static void waitTransferComplete() {
        DMA::waitTransferComplete();
    }

    static void doDoubleBufferTransfer() {
        DMA::doDoubleBufferTransfer();
    }
};

//******************************************************************************
// ILI9341 driver
//******************************************************************************
/* Use COLOR_FILTER_BGR=true for LCD with BGR color filter panel, false for RGB,
   if set incorrectly, color format RGB/BGR will be swapped. */
template<class LL_IF, bool COLOR_FILTER_BGR=true>
class Ili9341_driver
{
    enum class Command
    {
        NOP = 0x00,
        SOFTWARE_RESET = 0x01,
        READ_DISPLAY_ID = 0x04,
        READ_DISPLAY_STATUS = 0x09,
        READ_DISPLAY_POWER_MODE = 0x0A,
        READ_DISPLAY_MADCTL = 0x0B,
        READ_DISPLAY_PIXEL_FORMAT = 0x0C,
        READ_DISPLAY_IMAGE_FORMAT = 0x0D,
        READ_DISPLAY_SIGNAL_MODE = 0x0E,
        READ_DISPLAY_SELF_DIAGNOSTIC_RESULT = 0x0F,
        ENTER_SLEEP_MODE = 0x10,
        EXIT_SLEEP_MODE = 0x11,
        PARTIAL_MODE_ON = 0x12,
        NORMAL_DISPLAY_MODE_ON = 0x13,
        INVERT_DISPLAY_OFF = 0x20,
        INVERT_DISPLAY_ON = 0x21,
        GAMMA_SET = 0x26,
        DISPLAY_OFF = 0x28,
        DISPLAY_ON = 0x29,
        COLUMN_ADDRESS_SET = 0x2A,
        ROW_ADDRESS_SET = 0x2B,
        MEMORY_WRITE = 0x2C,
        MEMORY_READ = 0x2E,
        PARTIAL_AREA = 0x30,
        VERTICAL_SCROLLING_DEFINITION = 0x33,
        TEARING_EFFECT_LINE_OFF = 0x34,
        TEARING_EFFECT_LINE_ON = 0x35,
        MEMORY_ACCESS_CONTROL = 0x36,
        VERTICAL_SCROLLING_START_ADDRESS = 0x37,
        IDLE_MODE_OFF = 0x38,
        IDLE_MODE_ON = 0x39,
        PIXEL_FORMAT_SET = 0x3A,
        WRITE_MEMORY_CONTINUE = 0x3C,
        READ_MEMORY_CONTINUE = 0x3E,
        SET_TEAR_SCANLINE = 0x44,
        GET_SCANLINE = 0x45,
        WRITE_DISPLAY_BRIGHTNESS = 0x51,
        READ_DISPLAY_BRIGHTNESS = 0x52,
        WRITE_CTRL_DISPLAY = 0x53,
        READ_CTRL_DISPLAY = 0x54,
        WRITE_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL = 0x55,
        READ_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL = 0x56,
        WRITE_CABC_MIN_BRIGHTNESS = 0x5E,
        READ_CABC_MIN_BRIGHTNESS = 0x5F,
        POWER_CONTROL_A = 0xCB,
        POWER_CONTROL_B = 0xCF,
        POSITIVE_GAMMA_CORRECTION = 0xE0,
        NEGATIVE_GAMMA_CORRECTION = 0xE1,
        DRIVER_TIMING_CONTROL_A = 0xE8,
        DRIVER_TIMING_CONTROL_B = 0xEA,
        POWER_ON_SEQUENCE_CONTROL = 0xED,
        PUMP_RATIO_CONTROL = 0xF7,
        POWER_CONTROL_1 = 0xC0,
        POWER_CONTROL_2 = 0xC1,
        VCM_CONTROL_1 = 0xC5,
        VCM_CONTROL_2 = 0xC7,
        MEMORY_ACCESS_CONTROL_2 = 0xB1,
        FRAME_RATE_CONTROL_NORMAL = 0xB2,
        FRAME_RATE_CONTROL_IDLE = 0xB3,
        DISPLAY_INVERSION_CONTROL = 0xB4,
        BLANKING_PORCH_CONTROL = 0xB5,
        DISPLAY_FUNCTION_CONTROL = 0xB6,
        ENTRY_MODE_SET = 0xB7,
        READ_ID1 = 0xDA,
        READ_ID2 = 0xDB,
        READ_ID3 = 0xDC
    };

    enum MadCtlFlags : uint8_t
    {
        MADCTL_MY = 0x80,
        MADCTL_MX = 0x40,
        MADCTL_MV = 0x20,
        MADCTL_ML = 0x10,
        MADCTL_RGB = 0x00,
        MADCTL_BGR = 0x08,
        MADCTL_MH = 0x04
    };

    static void writeCommand(Command cmd, bool doSelect=true);

    static inline uint32_t s_width = 0;
    static inline uint32_t s_height = 0;
    static inline uint32_t s_dmaTransferLinesPerTransfer = 0;
    static inline uint32_t s_dmaTransferLine = 0;
public:
    enum class ColorFormat
    {
        RGB565,
        BGR565
    };

    enum class DisplayMode
    {
        NORMAL,
        INVERTED
    };

    enum class Orientation
    {
        PORTRAIT,
        LANDSCAPE,
        PORTRAIT_FLIPPED,
        LANDSCAPE_FLIPPED
    };

    static no_inline void init(Orientation orientation = Orientation::PORTRAIT, ColorFormat color_format=ColorFormat::RGB565);
    static uint32_t getWidth() { return s_width; }
    static uint32_t getHeight() { return s_height; }

    // Interface control
    static void dmaMode();
    static void controlMode();
    static void restartCs(uint32_t pulse_width_clk);
    static void setupCircularDoubleBufferMode(uint8_t* buff, size_t buff_size);
    static void startTransfer();
    static void waitTransferComplete();
    static void doDoubleBufferTransfer();

    // Low-level commands
    static void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    static void setXWindow(uint32_t x0, uint32_t x1);
    static void setYWindow(uint32_t y0, uint32_t y1);
    static void memoryWrite();
    static void invertColors(bool invert);

    // Drawing functions
    static void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    static void fillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    static void fillScreen(uint16_t color);
    static void drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
    static void writeChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
    static void writeString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
};

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::dmaMode()
{
    LL_IF::dataMode();
    LL_IF::dmaMode();
    LL_IF::select();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::controlMode()
{
    LL_IF::controlMode();
    LL_IF::select();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::setupCircularDoubleBufferMode(uint8_t* buff, size_t buff_size)
{
    dmaMode();
    LL_IF::setupCircularDoubleBufferMode(buff, buff_size);
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::startTransfer() {
    LL_IF::startTransfer();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::waitTransferComplete() {
    LL_IF::waitTransferComplete();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::doDoubleBufferTransfer() {
    LL_IF::doDoubleBufferTransfer();
    bool resetAddressWindow = true;
    if (resetAddressWindow)
    {
        restartCs(10);
        controlMode();
        //setAddressWindow(32, scanline, 32 + 255, 239);
        dmaMode();
    }
    startTransfer();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::writeCommand(Command cmd, bool doSelect)
{
    LL_IF::writeCommand(static_cast<uint8_t>(cmd), doSelect);
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::restartCs(uint32_t pulse_width_clk)
{
    LL_IF::unselect();
    for (uint32_t i = 0; i < pulse_width_clk; i++) __NOP();
    LL_IF::select();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::init(Orientation orientation, ColorFormat color_format)
{
    LL_IF::init();

    const uint32_t RESET_PULSE_MS = 5;
    LL_IF::controlMode();
    LL_IF::select();
    LL_IF::reset(RESET_PULSE_MS);

    // command list is based on https://github.com/martnak/STM32-ILI9341

    writeCommand(Command::SOFTWARE_RESET);
    HAL_Delay(1000);
        
    writeCommand(Command::POWER_CONTROL_A);
    {
        uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::POWER_CONTROL_B);
    {
        uint8_t data[] = { 0x00, 0xC1, 0x30 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::DRIVER_TIMING_CONTROL_A);
    {
        uint8_t data[] = { 0x85, 0x00, 0x78 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::DRIVER_TIMING_CONTROL_B);
    {
        uint8_t data[] = { 0x00, 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::POWER_ON_SEQUENCE_CONTROL);
    {
        uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::PUMP_RATIO_CONTROL);
    {
        uint8_t data[] = { 0x20 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::POWER_CONTROL_1);
    {
        uint8_t data[] = { 0x23 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::POWER_CONTROL_2);
    {
        uint8_t data[] = { 0x10 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::VCM_CONTROL_1);
    {
        uint8_t data[] = { 0x3E, 0x28 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::VCM_CONTROL_2);
    {
        uint8_t data[] = { 0x86 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::MEMORY_ACCESS_CONTROL);
    {
        uint8_t data[] = { 0x48 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::PIXEL_FORMAT_SET);
    {
        uint8_t data[] = { 0x55 };
        LL_IF::writeData(data, sizeof(data));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    writeCommand(static_cast<Command>(0xB1));
    {
        uint8_t data[] = { 0x00, 0x18 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::DISPLAY_FUNCTION_CONTROL);
    {
        uint8_t data[] = { 0x08, 0x82, 0x27 };
        LL_IF::writeData(data, sizeof(data));
    }

    // 3GAMMA FUNCTION DISABLE
    writeCommand(static_cast<Command>(0xF2));
    {
        uint8_t data[] = { 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::GAMMA_SET);
    {
        uint8_t data[] = { 0x01 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::POSITIVE_GAMMA_CORRECTION);
    {
        uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                           0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::NEGATIVE_GAMMA_CORRECTION);
    {
        uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                           0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
        LL_IF::writeData(data, sizeof(data));
    }

    writeCommand(Command::EXIT_SLEEP_MODE);
    HAL_Delay(120);

    writeCommand(Command::DISPLAY_ON);

    writeCommand(Command::MEMORY_ACCESS_CONTROL);
    {
        uint8_t madctl = 0x00;
        switch (orientation)
        {
            case Orientation::PORTRAIT:
                madctl = MADCTL_MX;
                s_width = 240;
                s_height = 320;
                break;
            case Orientation::LANDSCAPE:
                madctl = MADCTL_MV;
                s_width = 320;
                s_height = 240;
                break;
            case Orientation::PORTRAIT_FLIPPED:
                madctl = MADCTL_MY;
                s_width = 240;
                s_height = 320;
                break;
            case Orientation::LANDSCAPE_FLIPPED:
                madctl = MADCTL_MX | MADCTL_MY | MADCTL_MV;
                s_width = 320;
                s_height = 240;
                break;
        }
        if ((color_format == ColorFormat::BGR565) != COLOR_FILTER_BGR) {
            madctl |= MADCTL_BGR;
        } else {
            madctl |= MADCTL_RGB;
        }
        uint8_t data[] = { madctl };
        LL_IF::writeData(data, sizeof(data));
    }

    LL_IF::unselect();    
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::setXWindow(uint32_t x0, uint32_t x1)
{
    writeCommand(Command::COLUMN_ADDRESS_SET);
    {
        uint8_t data[] = { 
            static_cast<uint8_t>((x0 >> 8u) & 0xFFu),
            static_cast<uint8_t>(x0 & 0xFFu),
            static_cast<uint8_t>((x1 >> 8u) & 0xFFu),
            static_cast<uint8_t>(x1 & 0xFFu)
        };
        LL_IF::writeData(data, sizeof(data));
    }
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::setYWindow(uint32_t y0, uint32_t y1)
{
    writeCommand(Command::ROW_ADDRESS_SET);
    {
        uint8_t data[] = { 
            static_cast<uint8_t>((y0 >> 8u) & 0xFFu), 
            static_cast<uint8_t>(y0 & 0xFFu),
            static_cast<uint8_t>((y1 >> 8u) & 0xFFu),
            static_cast<uint8_t>(y1 & 0xFFu)
        };
        LL_IF::writeData(data, sizeof(data));
    }
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::memoryWrite()
{
    writeCommand(Command::MEMORY_WRITE);
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    setXWindow(x0, x1);
    setYWindow(y0, y1);
    memoryWrite();
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if((x >= getWidth()) || (y >= getHeight())) return;

    setAddressWindow(x, y, x, y);
    uint8_t data[] = { color >> 8, color & 0xFF };
    LL_IF::writeData(data, sizeof(data), false);
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::fillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if((x >= getWidth()) || (y >= getHeight())) return;
    if((x + w - 1) >= getWidth()) w = getWidth() - x;
    if((y + h - 1) >= getHeight()) h = getHeight() - y;

    setAddressWindow(x, y, x+w-1, y+h-1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    for(uint32_t i = 0; i < (w * h); i++)
    {
        LL_IF::writeData(data, sizeof(data), false);
    }
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::fillScreen(uint16_t color)
{
    fillRectangle(0, 0, getWidth(), getHeight(), color);
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data)
{
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::invertColors(bool invert)
{
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::writeChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
}

template<class LL_IF, bool COLOR_FILTER_BGR>
void Ili9341_driver<LL_IF, COLOR_FILTER_BGR>::writeString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor)
{
}

#include "ili9341_config.h"
