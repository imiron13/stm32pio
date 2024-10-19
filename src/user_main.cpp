#include "platform/cube_mx_stm32f103rc/main.h"
#include <stdio.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>
#include <shell.h>
#include <vt100_terminal.h>
#include <tetris.h>

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

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
#if (EN_TETRIS == 1)
    shell.add_command(ShellCmd_t("tetris", "Tetris!", shell_cmd_tetris));
#endif    
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
    }   
}
