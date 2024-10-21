#include "main.h"
#include <stdio.h>
#include <string.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>
#include <shell.h>
#include <vt100_terminal.h>
#include <tetris.h>
#include <usbd_cdc_if.h>
#include <usb_vcom_stdio_stm32.h>

#define EN_TETRIS                   (1)

extern UART_HandleTypeDef huart1;

Shell_t shell;

template<int port_id, int pin_id>
class GpioPin_t
{
    constexpr static GPIO_TypeDef *const gpio[5] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE };
public:
    static void writeHigh()
    {
        HAL_GPIO_WritePin(gpio[port_id], 1U << pin_id, GPIO_PIN_SET);
    }

    static void writeLow()
    {
        HAL_GPIO_WritePin(gpio[port_id], 1U << pin_id, GPIO_PIN_RESET);
    }

    static bool read()
    {
        return HAL_GPIO_ReadPin(gpio[port_id], 1U << pin_id);
    }

    static bool is_high()
    {
        return read();
    }
};

const int PORT_ID_DUMMY = INT32_MAX;
template<int port_id, int pin_id, bool is_inverted_polarity>
class Led_t
{
    GpioPin_t<port_id, pin_id> led_pin;
public:
    void on()
    {
        if (port_id != PORT_ID_DUMMY)
        {
            if (is_inverted_polarity)
            {
                led_pin.writeLow();
            }
            else
            {
                led_pin.writeHigh();
            }
        }
    }

    void off()
    {
        if (port_id != PORT_ID_DUMMY)
        {
            if (is_inverted_polarity)
            {
                led_pin.writeHigh();
            }
            else
            {
                led_pin.writeLow();
            }
        }
    }
};

typedef Led_t<PORT_ID_DUMMY, 0, false> DummyLed_t;

template<int port_id, int pin_id, bool is_inverted_polarity>
class Button_t
{
    GpioPin_t<port_id, pin_id> button_pin;
public:
    bool is_pressed()
    {
        if (port_id == PORT_ID_DUMMY)
        {
            return false;
        }
        else
        {
            if (is_inverted_polarity)
            {
                return button_pin.is_high() == false;
            }
            else
            {
                return button_pin.is_high();
            }
        }
    }
};

typedef Button_t<PORT_ID_DUMMY, 0, 0> DummyButton_t;

#define STM32F401RC (1)
#define STM32G431CB (2)
#define STM32F103ZE (3)

#if (MCU == STM32F401RC)
Led_t<2, 13, true> led1;
Button_t<0, 0, true> button1;
#elif (MCU == STM32G431CB)
Led_t<2, 6, false> led1;
Button_t<2, 13,false> button2;
Button_t<1, 8,false> button1;
#elif (MCU == STM32F103ZE)
Led_t<1, 9, true> led1;  // D0-PB9
Led_t<4, 5, true> led2;  // D1 PE5
Button_t<4, 4,true> button1;  // KO
Button_t<0, 0,false> button2;  // WAKE_UP
#elif (MCU == STM32F103RC)
DummyLed_t led1;
DummyButton_t button1;
#endif

bool shell_cmd_clear_screen(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME);
    return true;
}

#if (EN_TETRIS == 1)
TetrisGame_t<16, 16> tetris;

bool shell_cmd_tetris(FILE *f, ShellCmd_t *cmd, const char *s)
{
    tetris.set_device(f);
    tetris.start_game();

    bool quit = false;
    while (!quit)
    {
        quit = tetris.run_ui();
        HAL_Delay(tetris.get_ui_update_period_ms());
    }
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    fprintf(f, "Thanks for playing!" ENDL);
    return true;
}
#endif  

bool shell_cmd_led(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int state = cmd->get_int_arg(s, 1);
    if (state == 0)
    {
        led1.off();
    }
    else
    {
        led1.on();
    }
    return true;
}

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
#if (EN_TETRIS == 1)
    shell.add_command(ShellCmd_t("tetris", "Tetris!", shell_cmd_tetris));
#endif    
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));

    shell.set_device(device);
    shell.print_prompt();
}

extern "C" int user_main(void)
{
    FILE *fuart1 = uart_fopen(&huart1);
    FILE *fusb_vcom = usb_vcom_fopen();
    stdout = fusb_vcom;

    HAL_Delay(2000);  // delay for USB reconnect
    printf(BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    printf(ENDL "Hello from %s!" ENDL, MCU_NAME_STR);
#ifdef DEBUG
    printf("DEBUG=1, build time: " __TIME__ ENDL);
#else
    printf("DEBUG=0, build time: " __TIME__ ENDL);
#endif

    init_shell();    

    while (1)
    {
        shell.run();

        if (button1.is_pressed())
        {
            printf("Button pressed" ENDL);
            led1.on();
            HAL_Delay(200);
        }
        else
        {
            led1.off();
        }
#if 0
        if (button2.is_pressed())
        {
            printf("Button pressed" ENDL);
            HAL_Delay(200);
            led2.on();
        }
        else
        {
            led2.off();
        }
#endif        
    }   
}
