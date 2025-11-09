#pragma once

#include "hal_wrapper_stm32.h"
#include "st7735.h"
#include "vt100_terminal.h"

class St7735_Vt100_t: public Vt100TerminalServer_t
{
    FontDef m_font;
    int m_cur_scroll_pos;
public:
    void init(FontDef font);

    virtual void print_char(char c);
    virtual RawColor_t rgb_to_raw_color(RgbColor_t rgb);
    virtual void scroll(int num_lines);
};

