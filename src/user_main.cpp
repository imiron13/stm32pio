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

#include <FreeRTOS.h>
#include "task.h"
#include "cmsis_os.h"
#include "usb_device.h"

#define EN_SD_CARD                                          (1 && (BOARD_HAS_SD_CARD == 1))
#define EN_FATFS                                            (1 && (EN_SD_CARD == 1))

#define EN_SD_CARD_READ_WRITE_SHELL_CMDS                    (1 && (EN_SD_CARD == 1))
#define EN_FATFS_SHELL_CMDS                                 (1 && (EN_FATFS == 1))

#define EN_TETRIS                                           (1)

#if (EN_FATFS == 1)
#include "ff.h"
FATFS fs;
#endif

static const uint32_t SD_CARD_BLOCK_SIZE_IN_BYTES = 512;  

extern UART_HandleTypeDef huart1;
#if (EN_SD_CARD == 1)
extern SD_HandleTypeDef hsd;
#endif

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

#if (EN_SD_CARD_READ_WRITE_SHELL_CMDS == 1)
uint8_t sd_card_buf[SD_CARD_BLOCK_SIZE_IN_BYTES];

bool shell_cmd_sd_card_read(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int blk = cmd->get_int_arg(s, 1);
    int num_bytes = cmd->get_int_arg(s, 2);
    HAL_StatusTypeDef res = HAL_SD_ReadBlocks(&hsd, (uint8_t *)sd_card_buf, blk, 1, HAL_MAX_DELAY);
    //int res = SDCARD_ReadSingleBlock(blk, sd_card_buf);
    if (res != HAL_OK)
     {
        fprintf(f, "Error reading SD card block %u, error = %d" ENDL, blk, (int)hsd.ErrorCode);
        return false;
    }
    else
    {
        for (int i = 0; i < num_bytes; i++)
        {
            fprintf(f, "%02X ", sd_card_buf[i]);
            if ((i % 8) == 7)
            {
                fprintf(f, ENDL);
            }
        }
        fprintf(f, ENDL);
        return true;
    }
}

bool shell_cmd_sd_card_write(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int blk = cmd->get_int_arg(s, 1);
    HAL_StatusTypeDef status = HAL_SD_WriteBlocks(&hsd, sd_card_buf, blk, 1, HAL_MAX_DELAY);

    if (status != HAL_OK)
    {
        fprintf(f, "Error writing SD card block %d, error = %d" ENDL, blk, (int)hsd.ErrorCode);
        return false;
    }
    else
    {
        fprintf(f, "Successfully written to SD card block %d" ENDL, blk);
        return true;
    }
}
#endif

#if (EN_FATFS_SHELL_CMDS == 1)
char current_folder[FF_MAX_LFN] = "/";

bool shell_cmd_ls(FILE *f, ShellCmd_t *cmd, const char *s)
{
    DIR dir;
    int res = f_opendir(&dir, current_folder);
    if(res != FR_OK) {
       fprintf(f, "f_opendir() failed, res = %d" ENDL, res);
        return false;
    }

    FILINFO fileInfo;
    uint32_t totalFiles = 0;
    uint32_t totalDirs = 0;
    for(;;) {
       res = f_readdir(&dir, &fileInfo);
       if((res != FR_OK) || (fileInfo.fname[0] == '\0')) {
           break;
       }

       if(fileInfo.fattrib & AM_DIR) {
           fprintf(f, "  DIR  %s" ENDL, fileInfo.fname);
           totalDirs++;
       } else {
           fprintf(f, "  FILE %s" ENDL, fileInfo.fname);
           totalFiles++;
       }
    }

    return true;
}

bool shell_cmd_pwd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, "%s" ENDL, current_folder);
    return true;
}

bool shell_cmd_cd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    bool is_success = true;
    DIR dir;
    const char *folder_arg = cmd->get_str_arg(s, 1);
    char *new_folder = (char*)malloc(FF_MAX_LFN);
    if (strcmp(folder_arg, "..") == 0)
    {
        int i = 0;
        int slash_pos = 0;
        while (current_folder[i] != 0)
        {
            if (current_folder[i] == '/')
            {
                slash_pos = i;
            }
            i++;
        }
        if (slash_pos == 0) slash_pos = 1;
        current_folder[slash_pos] = 0;
    }
    else
    {
        if (folder_arg[0] != '/')
        {
            strcpy(new_folder, current_folder);
            if (current_folder[strlen(current_folder)- 1] != '/')
            {
                strcat(new_folder, "/");
            }
        }

        strcat(new_folder, folder_arg);
        int res = f_opendir(&dir, new_folder);

        if(res != FR_OK) {
            fprintf(f, "Invalid path" ENDL);
            is_success = false;
        }
        else
        {
            strcpy(current_folder, new_folder);

        }
    }
    free(new_folder);
    return is_success;
}
#endif

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
#if (EN_TETRIS == 1)
    shell.add_command(ShellCmd_t("tetris", "Tetris!", shell_cmd_tetris));
#endif
#if (EN_SD_CARD_READ_WRITE_SHELL_CMDS == 1)
    shell.add_command(ShellCmd_t("sdrd", "SD card read", shell_cmd_sd_card_read));
    shell.add_command(ShellCmd_t("sdwr", "SD card write", shell_cmd_sd_card_write));
#endif
#if (EN_FATFS_SHELL_CMDS == 1)
    shell.add_command(ShellCmd_t("ls", "Print conents of the current directory", shell_cmd_ls));
    shell.add_command(ShellCmd_t("cd", "Change directory", shell_cmd_cd));
    shell.add_command(ShellCmd_t("pwd", "Print current directory", shell_cmd_pwd));
#endif    
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));

    shell.set_device(device);
    shell.print_prompt();
}

#if (EN_FATFS == 1)
bool init_fatfs()
{
    printf("FATFS init...");
    FRESULT res;

    // mount the default drive
    res = f_mount(&fs, "", 0);
    if(res != FR_OK) {
        printf("f_mount() failed, res = %d" ENDL, res);
        return false;
    }

    uint32_t freeClust;
    FATFS* fs_ptr = &fs;
    // Warning! This fills fs.n_fatent and fs.csize!
    res = f_getfree("", &freeClust, &fs_ptr);
    if(res != FR_OK) {
        printf("f_getfree() failed, res = %d" ENDL, res);
        return false;
    }

    uint32_t totalBlocks = (fs.n_fatent - 2) * fs.csize;
    uint32_t freeBlocks = freeClust * fs.csize;
    printf("done" ENDL);
    printf("Total blocks: %lu (%lu MiB), free blocks: %lu (%lu MiB), cluster=%lu B" ENDL,
                totalBlocks, totalBlocks / 2048,
                freeBlocks, freeBlocks / 2048,
                fs.csize * SD_CARD_BLOCK_SIZE_IN_BYTES);
    return true;
}   
#endif

bool init_storage()
{
#if (EN_SD_CARD == 1)    
#if (EN_FATFS == 1)
    return init_fatfs();
#else
    printf("SD card init...");
    HAL_SD_CardInfoTypeDef card_info;
    HAL_SD_GetCardInfo(&hsd, &card_info);

    printf("done, num_blks=%lu, blk_size=%lu" ENDL, card_info.BlockNbr, card_info.BlockSize);
    return true;
#endif
#else
    return false;
#endif
}

osThreadId_t task_handle_user_input;

extern "C" void task_user_input(void *argument);

extern "C" void init()
{
    osThreadAttr_t defaultTask_attributes = { };
    defaultTask_attributes.name = "task_user_input";
    defaultTask_attributes.stack_size = 512 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);
}

extern "C" void task_user_input(void *argument)
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
    init_storage();
    init_shell();  

    for(;;)
    {
        shell.run();

        button1.update();
        button2.update();

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

        osDelay(50);
    }      
}
