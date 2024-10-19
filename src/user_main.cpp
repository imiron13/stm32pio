#include "main.h"
#include <stdio.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>
#include <shell.h>
#include <vt100_terminal.h>
#include <tetris.h>

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
};

GpioPin_t<2, 13> led_pin;
GpioPin_t<0, 0> button_pin;

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
        led_pin.writeHigh();
    }
    else
    {
        led_pin.writeLow();
    }
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
    stdout = fuart1;

    fprintf(fuart1, ENDL "Hello from %s!" ENDL, MCU_NAME_STR);
#ifdef DEBUG
    printf("DEBUG=1, build time: " __TIME__ ENDL);
#else
    printf("DEBUG=0, build time: " __TIME__ ENDL);
#endif

    init_shell();    

    while (1)
    {
        shell.run();

        if (button_pin.read() == 0)
        {
            led_pin.writeLow();
        }
        else
        {
            led_pin.writeHigh();
        }
    }   
}
