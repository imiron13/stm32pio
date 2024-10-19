#include <ctype.h>
#include "vt100_terminal.h"

const RgbColor_t Vt100TerminalServer_t::colors_table_rgb[] = {
    {{   0,   0,   0 }},
    {{ 205,   0,   0 }},
    {{   0, 205,   0 }},
    {{ 205, 205,   0 }},
    {{   0,   0, 238 }},
    {{ 205,   0, 205 }},
    {{   0, 205, 205 }},
    {{ 229, 229, 229 }},
    {{ 127, 127, 127 }},
    {{ 255,   0,   0 }},
    {{   0, 255,   0 }},
    {{ 255, 255,   0 }},
    {{  92,  92, 255 }},
    {{ 255,   0, 255 }},
    {{   0, 255, 255 }},
    {{ 255, 255, 255 }}
};

Vt100TerminalServer_t::Vt100TerminalServer_t()
{
}

void Vt100TerminalServer_t::init(int width, int height)
{
    m_width = width;
    m_height = height;
    reset();
}

void Vt100TerminalServer_t::cursor_home()
{
    set_pos(START_X_POS, START_Y_POS);
}

void Vt100TerminalServer_t::set_pos(int x, int y)
{
    m_x = x;
    m_y = y;
}

RgbColor_t Vt100TerminalServer_t::vt100_color_index_to_rgb(ColorIndex_t vt100_color_index)
{
    return colors_table_rgb[vt100_color_index];
}

Vt100TerminalServer_t::RawColor_t Vt100TerminalServer_t::vt100_color_index_to_raw(ColorIndex_t vt100_color_index)
{
    RgbColor_t rgb = vt100_color_index_to_rgb(vt100_color_index);
    return rgb_to_raw_color(rgb);
}

void Vt100TerminalServer_t::set_text_color(ColorIndex_t vt100_color_index)
{
    m_raw_text_color = vt100_color_index_to_raw(vt100_color_index);
}

void Vt100TerminalServer_t::set_background_color(ColorIndex_t vt100_color_index)
{
    m_raw_bg_color = vt100_color_index_to_raw(vt100_color_index);
}

void Vt100TerminalServer_t::hide_cursor()
{
    m_cursor_enabled = false;
}

void Vt100TerminalServer_t::show_cursor()
{
    m_cursor_enabled = true;
}

void Vt100TerminalServer_t::scroll(int num_lines)
{
    // not supported
}

void Vt100TerminalServer_t::clear_line(int line_num)
{
    int old_x = m_x;
    int old_y = m_y;
    m_y = line_num;
    for (m_x = 1; m_x <= m_width; m_x++)
    {
        print_char(' ');
    }
    m_x = old_x;
    m_y = old_y;
}

void Vt100TerminalServer_t::clear_screen()
{
    for (int y = 1; y <= m_height; y++)
    {
        clear_line(y);
    }
    cursor_home();
}

void Vt100TerminalServer_t::reset_parser()
{
    m_parser_state = VT100_PARSER_STATE_DEFAULT;
    m_args_parser_state = ARGS_PARSER_STATE_WAIT_DIGIT1;
    m_num_args = 0;
    for (int i = 0; i < MAX_ARGS; i++)
    {
        m_args[i] = ARG_NOT_SPECIFIED;
    }
}

void Vt100TerminalServer_t::reset()
{
    reset_parser();
    cursor_home();
    set_text_color(DEFAULT_TEXT_COLOR_INDEX);
    set_background_color(DEFAULT_BACKGROUND_COLOR_INDEX);
    show_cursor();
}

void Vt100TerminalServer_t::handle_escape_sequence_end(char c)
{
    // See https://en.wikipedia.org/wiki/ANSI_escape_code
    if (m_parser_state == VT100_PARSER_STATE_ESCAPE_RECEIVED)
    {
        // non CSI sequence - ignore
    }
    else if (m_parser_state == VT100_PARSER_STATE_CSI_RECEIVED)
    {
        switch (c)
        {
        case 'H':
            if (m_args[0] == ARG_NOT_SPECIFIED)
            {
                m_args[0] = START_Y_POS;
            }
            if (m_args[1] == ARG_NOT_SPECIFIED)
            {
                m_args[1] = START_X_POS;
            }

            set_pos(m_args[1], m_args[0]);  // first arg is y, second is x
            break;
        case 'm':
            // Select Graphic Rendition (SGR)
            if (m_args[0] >= 30 && m_args[0] <= 37)
            {
                set_text_color((ColorIndex_t)(m_args[0] - 30));
            }
            else if (m_args[0] >= 40 && m_args[0] <= 47)
            {
                set_background_color((ColorIndex_t)(m_args[0] - 40));
            }
            else if (m_args[0] >= 90 && m_args[0] <= 97)
            {
                set_text_color((ColorIndex_t)(VT100_COLOR_INDEX_BRIGHT + m_args[0] - 90));
            }
            else if (m_args[0] >= 100 && m_args[0] <= 107)
            {
                set_background_color((ColorIndex_t)(VT100_COLOR_INDEX_BRIGHT + m_args[0] - 100));
            }
            break;
        case 'J':
            // Erase in Display
            // Ignore the argument, handle all erase types as erase entire display
            clear_screen();
            break;
        case 'S':
            scroll(m_args[0]);
            break;
        }
    }
    else if (m_parser_state == VT100_PARSER_STATE_CSI_PRIVATE_RECEIVED)
    {
        switch (c)
        {
        case 'h':
            if (m_args[0] == 25) show_cursor();
            break;
        case 'l':
            if (m_args[0] == 25) hide_cursor();
            break;
        }
    }
    reset_parser();
}

void Vt100TerminalServer_t::handle_ascii(char c)
{
    if (c == '\n')
    {
        if (m_cursor_enabled)
        {
            print_char(' ');
        }
        m_x = START_X_POS;
        if (m_y == m_height)
        {
            scroll(1);
        }
        m_y++;
    }
    else if (c == '\b')
    {
        if (m_x > START_X_POS)
        {
            if (m_cursor_enabled)
            {
                print_char(' ');
            }
            m_x--;
            if (m_cursor_enabled)
            {
                print_char('_');
            }
        }
    }
    else
    {
        print_char(c);
        m_x++;
        if (m_cursor_enabled)
        {
            print_char('_');
        }
    }
}

bool Vt100TerminalServer_t::is_csi_termination_char(char c)
{
    return c >= 0x40 && c <= 0x7E;
}

void Vt100TerminalServer_t::handle_char(char c)
{
    switch (m_parser_state)
    {
    case VT100_PARSER_STATE_DEFAULT:
        if (c == CHAR_ESC)
        {
            m_parser_state = VT100_PARSER_STATE_ESCAPE_RECEIVED;
        }
        else
        {
            handle_ascii(c);
        }
        break;
    case VT100_PARSER_STATE_ESCAPE_RECEIVED:
        if (c == CHAR_CSI)
        {
            m_parser_state = VT100_PARSER_STATE_CSI_RECEIVED;
        }
        else
        {
            handle_escape_sequence_end(c);
        }
        break;
    case VT100_PARSER_STATE_CSI_RECEIVED:
        if (c == CHAR_CSI_PRIVATE)
        {
            m_parser_state = VT100_PARSER_STATE_CSI_PRIVATE_RECEIVED;
        }
        else if (c < 0x30 || c > 0x3F)
        {
            handle_escape_sequence_end(c);
        }
        break;
    case VT100_PARSER_STATE_CSI_PRIVATE_RECEIVED:
        if (c < 0x30 || c > 0x3F)
        {
            handle_escape_sequence_end(c);
        }
        break;
    }

    if (m_parser_state != VT100_PARSER_STATE_DEFAULT && isdigit(c))
    {
        if (m_args_parser_state == ARGS_PARSER_STATE_WAIT_DIGIT1)
        {
            m_args[m_num_args] = 0;
            m_num_args++;
        }

        m_args[m_num_args - 1] = 10 * m_args[m_num_args - 1] + (int)(c - '0');

        if (m_args_parser_state == ARGS_PARSER_STATE_WAIT_LAST_DIGIT)
        {
            m_args_parser_state = ARGS_PARSER_STATE_WAIT_DIGIT1;
        }
        else
        {
            m_args_parser_state = (ArgsParserState_t)((int)m_args_parser_state + 1);
        }
    }
    else
    {
        m_args_parser_state = ARGS_PARSER_STATE_WAIT_DIGIT1;
    }
}

extern "C" ssize_t vt100_write(void *vt100, const char *ptr, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        ((Vt100TerminalServer_t*)vt100)->handle_char(ptr[i]);
    }
    return len;
}

extern "C" ssize_t vt100_read(void *vt100_ptr, char* buff, size_t len)
{
    Vt100TerminalServer_t* vt100 = (Vt100TerminalServer_t*)vt100_ptr;
    return vt100->get_read_func()(vt100->get_read_cookie(), buff, len);
}

FILE *Vt100TerminalServer_t::fopen(cookie_read_function_t *read_func, void *read_cookie)
{
    m_read_func = read_func;
    m_read_cookie = read_cookie;
    cookie_io_functions_t cookie_funcs = {
        .read = vt100_read,
        .write = vt100_write,
        .seek = 0,
        .close = 0
    };
    FILE *f = fopencookie(this, "a+", cookie_funcs);
    setbuf(f, NULL);
    setvbuf(f, NULL, _IONBF, 0);
    return f;
}
