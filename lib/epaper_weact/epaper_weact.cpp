#include <epaper_weact.h>
#include <gpio_pin.h>
#include <hal_wrapper_stm32.h>
#include <stdint.h>
#include <stdio.h>
#include <intrinsics.h>

EpaperWeAct_Driver_t::EpaperWeAct_Driver_t()
{
}

void EpaperWeAct_Driver_t::config_pins(GpioPinInterface_t *scl, GpioPinInterface_t *sda,
             GpioPinInterface_t *dc, GpioPinInterface_t *cs, 
             GpioPinInterface_t *busy, GpioPinInterface_t *rst)
{
    m_scl = scl;
    m_sda = sda;
    m_dc = dc;
    m_cs = cs;
    m_busy = busy;
    m_rst = rst;
}

void EpaperWeAct_Driver_t::config_resolution(int width, int height)
{
    m_width = width;
    m_height = height;
}

void EpaperWeAct_Driver_t::select_cs()
{
    m_cs->write_low();
}

void EpaperWeAct_Driver_t::deselect_cs()
{
    m_cs->write_high();
}

void EpaperWeAct_Driver_t::reset_activate()
{
    m_rst->write_low();
}

void EpaperWeAct_Driver_t::reset_deactivate()
{
    m_rst->write_high();
}

void EpaperWeAct_Driver_t::select_command()
{
    m_dc->write_low();
}

void EpaperWeAct_Driver_t::select_data()
{
    m_dc->write_high();
}

void EpaperWeAct_Driver_t::sda_input()
{
    m_sda->config_input();
}

void EpaperWeAct_Driver_t::sda_output()
{
    m_sda->config_output();
}

void EpaperWeAct_Driver_t::send_byte(uint8_t byte)
{
    for (int i = BITS_PER_BYTE - 1; i >= 0; i--)
    {
        m_scl->write_low();
        m_sda->write((byte & (1U << i)) != 0);
        m_scl->write_high();
    }
}

uint8_t EpaperWeAct_Driver_t::read_byte()
{
    uint8_t data = 0;

    for (int i = BITS_PER_BYTE - 1; i >= 0; i--)
    {
        m_scl->write_low();
        //HAL_Delay(1);
        data |= static_cast<uint8_t>(m_sda->read()) << i;
        m_scl->write_high();
    }

    return data;    
}

void EpaperWeAct_Driver_t::send_command(uint8_t command)
{
    select_command();
    send_byte(command);
}

void EpaperWeAct_Driver_t::send_data(uint8_t data)
{
    select_data();
    send_byte(data);
}

uint8_t EpaperWeAct_Driver_t::read_data()
{
    select_data();
    return read_byte();
}

void EpaperWeAct_Driver_t::wait_idle()
{
    if (m_busy == nullptr)
    {
        HAL_Delay(DELAY_WHEN_BUSY_SIGNAL_NOT_AVAILABLE_MS);
        return;
    }

    while (m_busy->read())
    {
        HAL_Delay(BUSY_POLLING_PERIOD_MS);
    }
}

void EpaperWeAct_Driver_t::hw_reset()
{
    if (m_rst == nullptr)
    {
        return;
    }    
    reset_activate();
    HAL_Delay(HW_RESET_ACTIVE_DELAY_MS);
    reset_deactivate();
    HAL_Delay(POST_HW_RESET_DELAY_MS);
 }

void EpaperWeAct_Driver_t::send_sequence(uint8_t command, const uint8_t *data, int len)
{
    select_cs();
    send_command(command);
    for (int i = 0; i < len; i++)
    {
        send_data(data[i]);
    }
    deselect_cs();
}

void EpaperWeAct_Driver_t::send(uint8_t command)
{
    send_sequence(command, nullptr, 0);
}

void EpaperWeAct_Driver_t::send(uint8_t command, uint8_t data)
{
    send_sequence(command, &data, 1);
}

void EpaperWeAct_Driver_t::send(uint8_t command, uint8_t data1, uint8_t data2)
{
    uint8_t data[2] = {data1, data2};
    send_sequence(command, data, 2);
}

void EpaperWeAct_Driver_t::send(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3)
{
    uint8_t data[3] = {data1, data2, data3};
    send_sequence(command, data, 3);
}

void EpaperWeAct_Driver_t::send(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4)
{
    uint8_t data[4] = {data1, data2, data3, data4};
    send_sequence(command, data, 4);
}

void EpaperWeAct_Driver_t::send_and_receive(uint8_t command, const uint8_t *data, int len, uint8_t *receive, int receive_len)
{
    select_cs();
    send_command(command);
    for (int i = 0; i < len; i++)
    {
        send_data(data[i]);
    }
    sda_input();
    for (int i = 0; i < receive_len; i++)
    {
        receive[i] = read_data();
    }
    deselect_cs();
    sda_output();
}

void EpaperWeAct_Driver_t::sw_reset()
{
    send(CMD_SW_RESET);
    HAL_Delay(SW_RESET_DELAY_MS);
    wait_idle();
}

void EpaperWeAct_Driver_t::set_resolution()
{
    send(CMD_SET_RAM_X_RANGE, 0, ((m_width - 1) / BITS_PER_BYTE) & 0xFF);
    send(CMD_SET_RAM_Y_RANGE, 0, 0, (m_height - 1) & 0xFF, (m_height - 1) >> 8);
}

void EpaperWeAct_Driver_t::reset()
{
    hw_reset();
    sw_reset();
}

void EpaperWeAct_Driver_t::select_internal_temperature_sensor()
{
    send(CMD_SELECT_TEMP_SENSOR, 0x80);
}

void EpaperWeAct_Driver_t::select_external_temperature_sensor()
{
    send(CMD_SELECT_TEMP_SENSOR, 0x48);
}

void EpaperWeAct_Driver_t::set_x(int x)
{
    send(CMD_SET_RAM_X_ADDR, x);
}

void EpaperWeAct_Driver_t::set_y(int y)
{
    send(CMD_SET_RAM_Y_ADDR, y & 0xFF, y >> 8);
}

void EpaperWeAct_Driver_t::set_xy(int x, int y)
{
    set_x(x);
    set_y(y);
}

void EpaperWeAct_Driver_t::init()
{
    // Configure pins
    m_busy->config_input();
    m_rst->config_output();
    m_dc->config_output();
    m_cs->config_output();
    m_scl->config_output();
    m_sda->config_output();

    m_cs->write_high();
    m_scl->write_high();
    m_sda->write_high();
    m_rst->write_high();
    m_dc->write_high();

    // 1. Power On
    HAL_Delay(POWER_ON_DELAY_MS);

    // 2. Set Initial Configuration
    reset();

    // 3. Send Initialization Code
    send(CMD_DRIVER_OUTPUT_CONTROL, 0x2B, 0x01, 0x00);
    send(CMD_SET_GATE_VOLTAGE, 0x15);
    send(CMD_SOFT_START_CONTROL, 0x8E, 0x8C, 0x85, 0x3F);

    send(CMD_SET_DATA_ENTRY_MODE, 0x03);
    set_resolution();

    send(CMD_BORDER_WAVEFORM_CONTROL, 0x05);

    // 4. Load Waveform LUT
    send(CMD_DISPLAY_UPDATE_CONTROL_1, 0x40, 0x00);  // Display update control

    send(CMD_MEM_READ_OPTIONS, 0x00, 0x08, 0x16);  // Memory read options
    set_xy(0, 0);
}

void EpaperWeAct_Driver_t::read_user_id(uint8_t *user_id)
{
    send_and_receive(CMD_READ_USER_ID, nullptr, 0, user_id, USER_ID_NUM_BYTES);
}

void EpaperWeAct_Driver_t::print_user_id()
{
    uint8_t user_id[USER_ID_NUM_BYTES];
    read_user_id(user_id);
    printf("User ID: ");
    for (int i = 0; i < USER_ID_NUM_BYTES; i++)
    {
        printf("%02X ", user_id[i]);
    }
    printf(ENDL);
}

void EpaperWeAct_Driver_t::print_mem(int size)
{
    set_xy(0, 0);
    select_cs();
    send_command(CMD_MEM_READ);
    //send_and_receive(CMD_MEM_READ, nullptr, 0, data, 512);
    sda_input();
    for (int i = 0; i < size; i++)
    {
        if ((i % 16) == 0)
        {
            printf(ENDL);
        }
        printf("%02X ", read_data());
    }
    printf(ENDL);
    deselect_cs();
    sda_output();
}

void EpaperWeAct_Driver_t::draw_pattern(int pattern_id)
{
    if (pattern_id < 0 || pattern_id > 6)
    {
        return;
    }
    set_xy(0, 0);
    uint8_t pattern = (pattern_id << 4) | pattern_id;
    send(CMD_GENERATE_PATTERN, pattern);
    wait_idle();
}

void EpaperWeAct_Driver_t::mem_write(uint8_t data)
{
    select_cs();
    send_command(CMD_MEM_WRITE);
    send_data(data);
    deselect_cs();
}

void EpaperWeAct_Driver_t::mem_write(int x, int y, uint8_t data)
{
    set_xy(x, y);
    mem_write(data);
}

void EpaperWeAct_Driver_t::draw_pattern_sw(int pattern_id)
{
    set_xy(0, 0);
    select_cs();
    send_command(CMD_MEM_WRITE);
    
    for (int j = 0; j < m_height; j++)
    {
        for (int i = 0; i < m_width / BITS_PER_BYTE; i++)
        {
            send_data(((i >> pattern_id) & 0x01) ^ ((j / BITS_PER_BYTE >> pattern_id) & 0x01) ? 0x00 : 0xFF);
        }
    }
    deselect_cs();
}

void EpaperWeAct_Driver_t::update(bool full_refresh)
{
    //set_xy(0, 0);
    if (full_refresh)
    {
        send(CMD_DISPLAY_UPDATE_CONTROL_2, 0xF7);  // Full update
    }
    else
    {
        send(CMD_DISPLAY_UPDATE_CONTROL_2, 0xC7);  // Partial update
    }

    send(CMD_REFRESH);  // Activate Display Update Sequence
    wait_idle();
}

void EpaperWeAct_Driver_t::draw_pixel(int x, int y, EpaperWeAct_Driver_t::Color_t color)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        return;
    }

    set_xy(x, y);
    // TBD
    //send_command(0x24);  // Write RAM for black(0)/white(1) dot
    //send_data((int)color << (x & 0x07));
}

void EpaperWeAct_Driver_t::draw_hline(int x, int y, int w, EpaperWeAct_Driver_t::Color_t color)
{
    for (int i = 0; i < w; i++)
    {
        draw_pixel(x + i, y, color);
    }
}

void EpaperWeAct_Driver_t::draw_vline(int x, int y, int h, EpaperWeAct_Driver_t::Color_t color)
{
    for (int i = 0; i < h; i++)
    {
        draw_pixel(x, y + i, color);
    }
}

void EpaperWeAct_Driver_t::draw_rect(int x, int y, int w, int h, EpaperWeAct_Driver_t::Color_t color)
{
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

void EpaperWeAct_Driver_t::draw_fill_rect(int x, int y, int w, int h, EpaperWeAct_Driver_t::Color_t color)
{
    for (int i = 0; i < h; i++)
    {
        draw_hline(x, y + i, w, color);
    }
}

void EpaperWeAct_Driver_t::clear(EpaperWeAct_Driver_t::Color_t color)
{
    uint8_t data = (color == WHITE ? 0xFF : 0x00);
    set_xy(0, 0);
    select_cs();
    send_command(CMD_MEM_WRITE);
    for (int i = 0; i < m_width / BITS_PER_BYTE * m_height; i++)
    {
        send_data(data);
    }
    deselect_cs();
}

void EpaperWeAct_Driver_t::sleep()
{
    send(CMD_POWER_MODE, DEEP_SLEEP_MODE2);
}

void EpaperWeAct_Driver_t::wake_up()
{
    init();
}
