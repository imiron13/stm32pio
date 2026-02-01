#pragma once

#include <gpio_pin.h>
#include <stdint.h>

// Controller: SSD1683
class BinaryScreenBuffer_t
{
protected:
    int m_width;
    int m_height;

public:
    BinaryScreenBuffer_t(int width, int height);

    virtual void mem_write(int x, int y, uint8_t data) = 0;
    virtual uint8_t mem_read(int x, int y) = 0;
    void set_pixel(int x, int y, bool color);
    int get_width() { return m_width; }
    int get_height() { return m_height; }
};

class EpaperWeAct_Driver_t : public BinaryScreenBuffer_t
{
    static const int BITS_PER_BYTE = 8;
    static const int USER_ID_NUM_BYTES = 10;
    // timings
    static const int POWER_ON_DELAY_MS = 10;
    static const int HW_RESET_ACTIVE_DELAY_MS = 200;
    static const int POST_HW_RESET_DELAY_MS = 200;
    static const int SW_RESET_DELAY_MS = 10;
    static const int BUSY_POLLING_PERIOD_MS = 100;
    static const int DELAY_WHEN_BUSY_SIGNAL_NOT_AVAILABLE_MS = 3000;

    enum Command_t
    {
        CMD_DRIVER_OUTPUT_CONTROL = 0x01,
        CMD_SET_GATE_VOLTAGE = 0x03,
        CMD_SOFT_START_CONTROL = 0x0C,
        CMD_POWER_MODE = 0x10,
        CMD_SET_DATA_ENTRY_MODE = 0x11,
        CMD_SW_RESET = 0x12,
        CMD_SELECT_TEMP_SENSOR = 0x18,
        CMD_REFRESH = 0x20,
        CMD_DISPLAY_UPDATE_CONTROL_1 = 0x21,
        CMD_DISPLAY_UPDATE_CONTROL_2 = 0x22,
        CMD_MEM_WRITE = 0x24,
        CMD_MEM_READ = 0x27,
        CMD_READ_USER_ID = 0x2E,
        CMD_MEM_READ_OPTIONS = 0x41,
        CMD_SET_RAM_X_RANGE = 0x44,
        CMD_SET_RAM_Y_RANGE = 0x45,
        CMD_BORDER_WAVEFORM_CONTROL = 0x3C,
        CMD_GENERATE_PATTERN = 0x47,
        CMD_SET_RAM_X_ADDR = 0x4E,
        CMD_SET_RAM_Y_ADDR = 0x4F,
        CMD_NOP = 0x7F,
    };

    enum DeepSleepMode_t
    {
        DEEP_SLEEP_MODE1 = 0x01,
        DEEP_SLEEP_MODE2 = 0x03,
    };
    GpioPinInterface *m_scl;
    GpioPinInterface *m_sda;
    GpioPinInterface *m_dc;
    GpioPinInterface *m_cs;
    GpioPinInterface *m_busy;
    GpioPinInterface *m_rst;


    void select_cs();
    void deselect_cs();
    void reset_activate();
    void reset_deactivate();
    void select_command();
    void select_data();
    void sda_input();
    void sda_output();

    void send_byte(uint8_t byte);
    uint8_t read_byte();
    void send_command(uint8_t command);
    void send_data(uint8_t data);
    uint8_t read_data();

    void hw_reset();
    void wait_idle();

    void send_sequence(uint8_t command, const uint8_t *data, int len);
    void send(uint8_t command);
    void send(uint8_t command, uint8_t data);
    void send(uint8_t command, uint8_t data1, uint8_t data2);
    void send(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3);
    void send(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4);
    void send_and_receive(uint8_t command, const uint8_t *data, int len, uint8_t *recieve, int recieve_len);

    void sw_reset();
    void set_resolution();
    void select_internal_temperature_sensor();
    void select_external_temperature_sensor();
    void reset();


    void set_x(int x);
    void set_y(int y);

public:
    enum Color_t
    {
        WHITE = 0,
        BLACK = 1
    };

    EpaperWeAct_Driver_t(int width, int height);

    void config_pins(GpioPinInterface *scl, GpioPinInterface *sda,
                     GpioPinInterface *dc, GpioPinInterface *cs, 
                     GpioPinInterface *busy, GpioPinInterface *rst);
    void config_resolution(int width, int height);

    void init();
    void clear(Color_t color=WHITE);

    void set_xy(int x, int y);

    void draw_pixel(int x, int y, Color_t color);
    void draw_hline(int x, int y, int w, Color_t color=BLACK);
    void draw_vline(int x, int y, int h, Color_t color=BLACK);
    void draw_rect(int x, int y, int w, int h, Color_t color=BLACK);
    void draw_fill_rect(int x, int y, int w, int h, Color_t color=BLACK);
    void draw_pattern(int pattern_id=4);
    void draw_pattern_sw(int pattern_id=4);
    void update(bool full_refresh=true);

    void read_user_id(uint8_t *user_id);
    void print_user_id();
    void print_mem(int size);

    void mem_write(uint8_t data);
    virtual void mem_write(int x, int y, uint8_t data);
    uint8_t mem_read();
    virtual uint8_t mem_read(int x, int y);

    void sleep();
    void wake_up();
};
