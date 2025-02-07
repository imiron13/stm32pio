#include "main.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <malloc.h>

#include <gpio_pin_stm32.h>
#include <board.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>

#include "usb_device.h"
#include <usb_vcom_stdio_stm32.h>

#include <gpio_pin.h>
#include <led.h>
#include <button.h>
#include <shell.h>
#include <vt100_terminal.h>

#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"

#include <keypad_4x1.h>

using namespace std;

#ifndef BOARD_SUPPORT_SD_CARD
#define BOARD_SUPPORT_SD_CARD                                   (0)
#endif

#ifndef BOARD_SUPPORT_AUDIO
#define BOARD_SUPPORT_AUDIO                                     (0)
#endif

#define EN_SD_CARD                                          (1 && (BOARD_SUPPORT_SD_CARD == 1))
#define EN_FATFS                                            (1 && (EN_SD_CARD == 1))

#define EN_SD_CARD_READ_WRITE_SHELL_CMDS                    (1 && (EN_SD_CARD == 1))
#define EN_FATFS_SHELL_CMDS                                 (1 && (EN_FATFS == 1))
#define EN_AUDIO_SHELL_CMDS                                 (1 && (BOARD_SUPPORT_AUDIO == 1))
#define EN_TETRIS                                           (1)

extern UART_HandleTypeDef huart1;
Shell_t shell;

bool shell_cmd_clear_screen(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME);
    return true;
}

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

bool shell_heap_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    struct mallinfo heap_info = mallinfo();
    fprintf(f, "arena=%d" ENDL, heap_info.arena);  /* Non-mmapped space allocated (bytes) */
    fprintf(f, "ordblks=%d" ENDL, heap_info.ordblks);  /* Number of free chunks */
    fprintf(f, "smblks=%d" ENDL, heap_info.smblks);  /* Number of free fastbin blocks */
    fprintf(f, "hblks=%d" ENDL, heap_info.hblks);  /* Number of mmapped regions */
    fprintf(f, "hblkhd=%d" ENDL, heap_info.hblkhd);  /* Space allocated in mmapped regions (bytes) */
    fprintf(f, "usmblks=%d" ENDL, heap_info.usmblks);  /* Maximum total allocated space (bytes) */
    fprintf(f, "fsmblks=%d" ENDL, heap_info.fsmblks);  /* Space in freed fastbin blocks (bytes) */
    fprintf(f, "uordblks=%d" ENDL, heap_info.uordblks);  /* Total allocated space (bytes) */
    fprintf(f, "fordblks=%d" ENDL, heap_info.fordblks);  /* Total free space (bytes) */
    fprintf(f, "keepcost=%d" ENDL, heap_info.keepcost);  /* Top-most, releasable space (bytes) */
    return true;
}

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));

    shell.add_command(ShellCmd_t("heap", "", shell_heap_stat));

    shell.set_device(device);
    shell.print_prompt();
}

extern "C" void task_user_input(void *argument);
osThreadId_t task_handle_user_input;

extern "C" void task_user_input(void *argument)
{
    GpioPinPortB_t<10> keypad_key2;
    GpioPinPortB_t<2> keypad_key1;
    GpioPinPortB_t<0> keypad_key4;
    GpioPinPortA_t<7> keypad_key3;

    Keypad_4x1 keypad(&keypad_key1, &keypad_key2, &keypad_key3, &keypad_key4);
    keypad.init();

    for(;;)
    {
        shell.run();

        button1.update();
        button2.update();
        keypad.button1().update();
        keypad.button2().update();
        keypad.button3().update();
        keypad.button4().update();

        if (button1.is_pressed_event())
        {
            printf("Button1 pressed" ENDL);
            led1.on();
        }
        else if (button1.is_released_event())
        {
            printf("Button1 released" ENDL);
            led1.off();
        }

        if (button2.is_pressed_event())
        {
            printf("Button2 pressed" ENDL);
            led2.on();
        }
        else if (button2.is_released_event())
        {
            printf("Button2 released" ENDL);
            led2.off();
        }

        if (keypad.button1().is_pressed_event())
        {
            printf("Keypad button1 pressed" ENDL);
            led1.on();
        }
        else if (keypad.button1().is_released_event())
        {
            printf("Keypad button1 released" ENDL);
            led1.off();
        }

        if (keypad.button2().is_pressed_event())
        {
            printf("Keypad button2 pressed" ENDL);
            led1.on();
        }
        else if (keypad.button2().is_released_event())
        {
            printf("Keypad button2 released" ENDL);
            led1.off();
        }

        if (keypad.button3().is_pressed_event())
        {
            printf("Keypad button3 pressed" ENDL);
            led1.on();
        }
        else if (keypad.button3().is_released_event())
        {
            printf("Keypad button3 released" ENDL);
            led1.off();
        }

        if (keypad.button4().is_pressed_event())
        {
            printf("Keypad button4 pressed" ENDL);
            led1.on();
        }
        else if (keypad.button4().is_released_event())
        {
            printf("Keypad button4 released" ENDL);
            led1.off();
        }

        osDelay(200);
    }       
}

extern "C" void init()
{
    MX_USB_DEVICE_Init();

    FILE *fuart1 = uart_fopen(&huart1);
    stdout = fuart1;

    FILE *fusb_vcom = usb_vcom_fopen();
    stdout = fusb_vcom;

    HAL_Delay(2000);  // delay for USB reconnect

    printf(BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    printf(ENDL "Hello from %s (FreeRTOS)!" ENDL, MCU_NAME_STR);
#ifdef DEBUG
    printf("DEBUG=1, build time: " __TIME__ ENDL);
#else
    printf("DEBUG=0, build time: " __TIME__ ENDL);
#endif
    printf("SysClk = %ld KHz" ENDL, HAL_RCC_GetSysClockFreq() / 1000);
    init_shell();  

    osThreadAttr_t defaultTask_attributes = { };
    defaultTask_attributes.name = "task_user_input";
    defaultTask_attributes.stack_size = 256 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);
}
