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

#if (EN_FATFS == 1)
#include "ff.h"
FATFS fs;
#endif

#if (BOARD_SUPPORT_AUDIO == 1)
#include "i2s.h"
int g_volume_shift = 0;
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

#if (EN_AUDIO_SHELL_CMDS == 1)
#include <math.h>

#define PI 3.14159265358979323846
#define TAU (2.0 * PI)

bool shell_cmd_test_dac(FILE *f, ShellCmd_t *cmd, const char *s) {
    HAL_StatusTypeDef res;
    int nsamples = 4096;
    int16_t *dac_signal = (int16_t*)malloc(nsamples * sizeof(int16_t));
    if (dac_signal == NULL)
    {
        fprintf(f, "Heap alloc error\n");
        return false;
    }

    int i = 0;
    while(i < nsamples) {
        double t = ((double)i/2.0)/((double)nsamples);
        dac_signal[i] = 1023*sin(100.0 * TAU * t); // left
        dac_signal[i+1] = dac_signal[i]; // right
        i += 2;
    }  

    bool end = false;
    while (!end) {
        res = HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)dac_signal, nsamples);
        /*res = HAL_I2S_Transmit(&hi2s2, (uint16_t*)dac_signal, nsamples,
                               HAL_MAX_DELAY);*/
        /*if(res != HAL_OK) {
            fprintf(f, "I2S - ERROR, res = %d!\r\n", res);
            return false;
        }*/

        int c = fgetc(f);
        if (c =='q')
        {
            //HAL_I2S_DMAStop();
            end = 1;
        }
    }
    free(dac_signal);
    return true;
} 

#include <memory>
using namespace std;

#define DAC_BUF_NUM_SAMPLES     (4096)
#define DAC_BUF_SIZE            (DAC_BUF_NUM_SAMPLES * sizeof(int16_t))

bool shell_cmd_open_wav(FILE *f, ShellCmd_t *cmd, const char *s)
{
    unique_ptr<uint16_t> signal_buff[2];
    signal_buff[0].reset(new uint16_t[DAC_BUF_SIZE / 2]);
    signal_buff[1].reset(new uint16_t[DAC_BUF_SIZE / 2]);

    const char* fname = cmd->get_str_arg(s, 1);
    fprintf(f, "Openning %s..." ENDL, fname);
    FIL file;
    FRESULT res = f_open(&file, fname, FA_READ);
    if(res != FR_OK) {
        fprintf(f, "f_open() failed, res = %d" ENDL, res);
        return -1;
    }

    fprintf(f, "File opened, reading..." ENDL);

    unsigned int bytesRead;
    uint8_t header[44+34];
    res = f_read(&file, header, sizeof(header), &bytesRead);
    if(res != FR_OK) {
        fprintf(f, "f_read() failed, res = %d" ENDL, res);
        f_close(&file);
        return -2;
    }

    if(memcmp((const char*)header, "RIFF", 4) != 0) {
        fprintf(f, "Wrong WAV signature at offset 0: "
                    "0x%02X 0x%02X 0x%02X 0x%02X" ENDL,
                    header[0], header[1], header[2], header[3]);
        f_close(&file);
        return -3;
    }

    if(memcmp((const char*)header + 8, "WAVEfmt ", 8) != 0) {
        fprintf(f, "Wrong WAV signature at offset 8!" ENDL);
        f_close(&file);
        return -4;
    }
    uint32_t data_ofs = 36;
    if(memcmp((const char*)header + data_ofs, "data", 4) != 0) {
        if (memcmp((const char*)header + data_ofs, "LIST", 4) == 0)
        {
            uint32_t listChunkSize = header[data_ofs + 4] | (header[data_ofs + 5] << 8) |
                        (header[data_ofs + 6] << 16) | (header[data_ofs + 7] << 24);
            data_ofs += 8 + listChunkSize;
        }
        else
        {
            fprintf(f, "Cannot find 'data' subchunk!" ENDL);
            f_close(&file);
            return -5;
        }
    }

    uint32_t fileSize = 8 + (header[4] | (header[5] << 8) |
                        (header[6] << 16) | (header[7] << 24));
    uint32_t headerSizeLeft = header[16] | (header[17] << 8) |
                              (header[18] << 16) | (header[19] << 24);
    uint16_t compression = header[20] | (header[21] << 8);
    uint16_t channelsNum = header[22] | (header[23] << 8);
    uint32_t sampleRate = header[24] | (header[25] << 8) |
                          (header[26] << 16) | (header[27] << 24);
    uint32_t bytesPerSecond = header[28] | (header[29] << 8) |
                              (header[30] << 16) | (header[31] << 24);
    uint16_t bytesPerSample = header[32] | (header[33] << 8);
    uint16_t bitsPerSamplePerChannel = header[34] | (header[35] << 8);
    uint32_t dataSize = header[data_ofs + 4] | (header[data_ofs + 5] << 8) |
                        (header[data_ofs + 6] << 16) | (header[data_ofs + 7] << 24);

    fprintf(f, 
        "--- WAV header ---" ENDL
        "File size: %lu" ENDL
        "Header size left: %lu" ENDL
        "Compression (1 = no compression): %d" ENDL
        "Channels num: %d" ENDL
        "Sample rate: %ld" ENDL
        "Bytes per second: %ld" ENDL
        "Bytes per sample: %d" ENDL
        "Bits per sample per channel: %d" ENDL
        "Data size: %ld" ENDL
        "------------------" ENDL,
        fileSize, headerSizeLeft, compression, channelsNum,
        sampleRate, bytesPerSecond, bytesPerSample,
        bitsPerSamplePerChannel, dataSize);

    if(headerSizeLeft != 16) {
        fprintf(f, "Wrong `headerSizeLeft` value, 16 expected" ENDL);
        f_close(&file);
        return -6;
    }

    if(compression != 1) {
        fprintf(f, "Wrong `compression` value, 1 expected" ENDL);
        f_close(&file);
        return -7;
    }

    if(channelsNum != 2) {
        fprintf(f, "Wrong `channelsNum` value, 2 expected" ENDL);
        f_close(&file);
        return -8;
    }

    if((sampleRate != 44100) || (bytesPerSample != 4) ||
       (bitsPerSamplePerChannel != 16) || (bytesPerSecond != 44100*2*2)
       || (dataSize < DAC_BUF_SIZE + DAC_BUF_SIZE)) {
        fprintf(f, "Wrong file format, 16 bit file with sample "
                    "rate 44100 expected" ENDL);
        f_close(&file);
        return -9;
    }

    res = f_read(&file, (uint8_t*)signal_buff[0].get(), DAC_BUF_SIZE,
                 &bytesRead);
    if(res != FR_OK) {
        fprintf(f, "f_read() failed, res = %d" ENDL, res);
        f_close(&file);
        return -10;
    }

    HAL_Delay(10);
    bool end = false;
    uint32_t play_buf_idx = 0;
    while(dataSize >= DAC_BUF_SIZE && !end) {
        res = f_read(&file, (uint8_t*)signal_buff[!play_buf_idx].get(),
                     DAC_BUF_SIZE, &bytesRead);
        int16_t *p = (int16_t*)signal_buff[!play_buf_idx].get();
        for (int i = 0; i < DAC_BUF_NUM_SAMPLES; i++)
        {
            p[i] >>= g_volume_shift;
        }
        if(res != FR_OK) {
            fprintf(f, "f_read() failed, res = %d" ENDL, res);
            f_close(&file);
            return -13;
        }
        int c = fgetc(f);
        if (c =='q')
        {
            end = true;
        }
        while (HAL_I2S_GetState(&hi2s2) == HAL_I2S_STATE_BUSY_TX) { };
        HAL_StatusTypeDef hal_res = HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)signal_buff[!play_buf_idx].get(),
                                  DAC_BUF_NUM_SAMPLES);
        play_buf_idx = !play_buf_idx;  
        dataSize -= DAC_BUF_SIZE;
    }

    res = f_close(&file);
    if(res != FR_OK) {
        fprintf(f, "f_close() failed, res = %d" ENDL, res);
        return -14;
    }

    return true;
}

bool shell_cmd_set_volume(FILE *f, ShellCmd_t *cmd, const char *s)
{
    g_volume_shift = cmd->get_int_arg(s, 1);
    fprintf(f, "Volume shift is %d now" ENDL, g_volume_shift);
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

#if (EN_AUDIO_SHELL_CMDS)    
    shell.add_command(ShellCmd_t("dac_test", "Test DAC", shell_cmd_test_dac));
    shell.add_command(ShellCmd_t("openwav", "Open a WAV file", shell_cmd_open_wav));
    shell.add_command(ShellCmd_t("volume", "Configure audio volume", shell_cmd_set_volume));
#endif
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
