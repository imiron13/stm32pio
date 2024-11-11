#include "main.h"
#include <stdio.h>
#include <string.h>
#include <memory>

#include <gpio_pin_stm32.h>
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
#include <audio_volume_control.h>
#include <wav_player.h>

#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include "usb_device.h"

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
    vTaskSuspendAll();
    int res = f_opendir(&dir, current_folder);
    xTaskResumeAll();
    if(res != FR_OK) {
       fprintf(f, "f_opendir() failed, res = %d" ENDL, res);
        return false;
    }

    FILINFO fileInfo;
    uint32_t totalFiles = 0;
    uint32_t totalDirs = 0;
    vTaskSuspendAll();
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
    xTaskResumeAll();
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
    return (res == HAL_OK);
} 

#define DAC_BUF_NUM_SAMPLES     (4096)
#define DAC_BUF_SIZE            (DAC_BUF_NUM_SAMPLES * sizeof(int16_t))

bool shell_cmd_open_wav(FILE *f, ShellCmd_t *cmd, const char *s)
{
    const char* fname = cmd->get_str_arg(s, 1);
    TaskPlayWav_t *play_wav = new TaskPlayWav_t;
    play_wav->start(fname);
    return true;
}

#endif

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
    defaultTask_attributes.stack_size = 256 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);
}

//------------------------------------------------------------------------------
class AudioPlayerController_t
{
public:
    enum State_t
    {
        STOPPED,
        PAUSED,
        PLAYING
    };
    AudioPlayerController_t();
    ~AudioPlayerController_t();
    void playSong(const char *wav_file_name);
    void playFolder();
    void pause();
    void stop();
    void play();
    void shuffle();
    void nextSong();
    void prevSong();
    void reserveSongsListCapacity(int capacity);
    void clearSongsList();
    bool is_paused();

    void setVolume();
    void getVolume();
    void printStat();

    const char *getCurrentSongName();
    const char *getCurrentSongFileName();
    int getTimestampMsec();
    void setTimestampMsec();

private:
    const char **m_songs_list;
    int m_num_songs;
    int m_songs_list_capacity;
    int m_cur_song_idx;
    int addSong(const char *wav_file_name);
    State_t m_state;
    unique_ptr<TaskPlayWav_t> m_task_play_wav;
    // in FIFO
};

AudioPlayerController_t::AudioPlayerController_t()
    : m_songs_list(nullptr)
    , m_num_songs(0)
    , m_songs_list_capacity(0)
    , m_cur_song_idx(0)
    , m_state(STOPPED)
{

}

AudioPlayerController_t::~AudioPlayerController_t()
{
    clearSongsList();
    delete m_songs_list;
}

void AudioPlayerController_t::printStat()
{
    if (m_task_play_wav)
    {
        m_task_play_wav->print_stat();
    }
}

void AudioPlayerController_t::clearSongsList()
{
    for (int i = 0; i < m_num_songs; i++)
    {
        delete m_songs_list[i];
    }
    m_num_songs = 0;
}

void AudioPlayerController_t::reserveSongsListCapacity(int capacity)
{
    if (m_songs_list_capacity <= capacity)
    {
        const char **new_songs_list = new const char*[capacity];
        for (int i = 0; i < m_songs_list_capacity; i++)
        {
            new_songs_list[i] = m_songs_list[i];
        }
        if (m_songs_list) delete m_songs_list;
        m_songs_list = new_songs_list;
        m_songs_list_capacity = capacity;
    }
}

bool AudioPlayerController_t::is_paused()
{
    return m_state == PAUSED;
}

void AudioPlayerController_t::nextSong()
{

}

void AudioPlayerController_t::playSong(const char *wav_file_name)
{
    clearSongsList();
    reserveSongsListCapacity(1);
    addSong(wav_file_name);
    play();
}

int AudioPlayerController_t::addSong(const char *wav_file_name)
{
    int len = strlen(wav_file_name);
    char *p = new char[len];
    if (p)
    {
        strcpy(p, wav_file_name);
        m_songs_list[m_num_songs] = p;
        int song_id = m_num_songs;
        m_num_songs++;
        return song_id;
    }
    else
    {
        printf("Memory allocation failure" ENDL);
        return INT32_MAX;
    }
}

void AudioPlayerController_t::shuffle()
{

}

void AudioPlayerController_t::pause()
{
    if (m_state == PLAYING)
    {
        m_task_play_wav->pause();
        printf("pause" ENDL);
        m_state = PAUSED;
    }
}

void AudioPlayerController_t::play()
{
    if (m_state == PAUSED)
    {
        m_task_play_wav->unpause();
        m_state = PLAYING;
    }
    else if (m_state == PLAYING)
    {
        stop();
    }

    if (m_state == STOPPED)
    {
        printf("play" ENDL);
        
        m_task_play_wav = unique_ptr<TaskPlayWav_t>(new TaskPlayWav_t);
        bool ok = m_task_play_wav->start(getCurrentSongFileName());

        if (ok)
        {
            m_state = PLAYING;
        }
    }

}

void AudioPlayerController_t::stop()
{
    m_task_play_wav->stop();
    m_task_play_wav.reset();
    m_state = STOPPED;
}

int AudioPlayerController_t::getTimestampMsec()
{
    return 0;
}

void AudioPlayerController_t::setTimestampMsec()
{

}

const char *AudioPlayerController_t::getCurrentSongFileName()
{
    return m_songs_list[m_cur_song_idx];
}

class AudioPlayerTerminalGui_t
{
public:
};

struct AudioPlayerInputs_t
{
    Button_t *button_pause;
    Button_t *button_next;
};

class AudioPlayerInputHandler_t
{
    AudioPlayerController_t *m_controller;
    AudioPlayerInputs_t m_inp;
public:
    AudioPlayerInputHandler_t(AudioPlayerController_t *controller, const AudioPlayerInputs_t &inputs);

    void handle_inputs();
};

AudioPlayerInputHandler_t::AudioPlayerInputHandler_t(AudioPlayerController_t *controller, const AudioPlayerInputs_t &inputs)
    : m_controller(controller)
    , m_inp(inputs)
{
}

void AudioPlayerInputHandler_t::handle_inputs()
{
    if (m_inp.button_pause->is_pressed_event())
    {
        if (m_controller->is_paused())
        {
            m_controller->play();
        }
        else
        {
            m_controller->pause();
        }
        m_controller->printStat();
    }

    if (m_inp.button_next->is_pressed_event())
    {
        m_controller->nextSong();
    }
}

class AudioPlayer_t
{
public:
    AudioPlayerController_t m_controller;
    AudioPlayerInputHandler_t m_input_handler;

    AudioPlayer_t(const AudioPlayerInputs_t &inputs);
};

AudioPlayer_t::AudioPlayer_t(const AudioPlayerInputs_t &inputs)
    : m_controller()
    , m_input_handler(&m_controller, inputs)
{

}

AudioPlayer_t *g_audio_player = nullptr;

//------------------------------------------------------------------------------
bool shell_cmd_play(FILE *f, ShellCmd_t *cmd, const char *s)
{
    if (g_audio_player)
    {
        fprintf(f, "Previous song is still playing, stopping..." ENDL);
        g_audio_player->m_controller.stop();
        delete g_audio_player;
    }

    AudioPlayerInputs_t inputs;
    inputs.button_pause = &button1;
    inputs.button_next = &button2;
    g_audio_player = new AudioPlayer_t(inputs);

    const char *fname = cmd->get_str_arg(s, 1);
    g_audio_player->m_controller.playSong(fname);
    return true;
}

bool shell_wav_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    if (g_audio_player)
    {
        g_audio_player->m_controller.printStat();
        return true;
    }
    return false;    
}

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

    shell.add_command(ShellCmd_t("play", "", shell_cmd_play));
    shell.add_command(ShellCmd_t("wavstat", "", shell_wav_stat));

    shell.set_device(device);
    shell.print_prompt();
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

        if (g_audio_player)
        {
            g_audio_player->m_input_handler.handle_inputs();
        }
        osDelay(50);
    }      
}
