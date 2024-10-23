#include "main.h"
#include <stdio.h>
#include <string.h>

#include <board.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>
#include <gpio_pin.h>
#include <led.h>
#include <button.h>
#include <shell.h>
#include <vt100_terminal.h>
#include <tetris.h>
#include <usb_vcom_stdio_stm32.h>

#define EN_TETRIS                   (1)

extern UART_HandleTypeDef huart1;

Shell_t shell;

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

void button1_on_pressed()
{
    printf("Button1 pressed" ENDL);
    led1.on();
    HAL_Delay(200);
}

void button2_on_pressed()
{
    printf("Button2 pressed" ENDL);
    HAL_Delay(200);
    led2.on();
}

extern "C" int user_main(void)
{
    FILE *fuart1 = uart_fopen(&huart1);
    UNUSED(fuart1);

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
    printf("SysClk = %ld KHz" ENDL, HAL_RCC_GetSysClockFreq() / 1000);

    init_shell();    

    while (1)
    {
        shell.run();

        if (button1.is_pressed())
        {
            button1_on_pressed();
        }
        else
        {
            led1.off();
        }

        if (button2.is_pressed())
        {
            button2_on_pressed();
        }
        else
        {
            led2.off();
        }
    }   
}
