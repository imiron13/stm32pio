#include "main.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <malloc.h>
#include <array>

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
#include <nes_apu.h>
#include <nes_cpu.h>
#include <nes_bus.h>
#include <nes_cartridge.h>
#include <vgm.h>

#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include "st7735.h"
#include "st7735_vt100.h"
#include "fonts.h"
#include "ili9341.h"
#include <keypad_4x1.h>
/*extern "C" {
#include <eeprom_emul.h>
}*/

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
#define EN_AUDIO_SHELL_CMDS                                 (0)
#define EN_TETRIS                                           (0)

extern UART_HandleTypeDef huart1;
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

Shell_t shell;

GpioPinPortA_t<6> keypad_gnd;
GpioPinPortA_t<4> keypad_key2;
GpioPinPortA_t<2> keypad_key1;
GpioPinPortA_t<0> keypad_key4;
GpioPinPortC_t<15> keypad_key3;


Keypad_4x1 keypad(&keypad_key1, &keypad_key2, &keypad_key3, &keypad_key4, &keypad_gnd);

#if (EN_AUDIO_SHELL_CMDS == 1)
#include <math.h>

#define PI 3.14159265358979323846
#define TAU (2.0 * PI)
#endif
//extern const uint8_t _binary_Unchained_vgm_start;
//extern const unsigned int _binary_Unchained_vgm_size;

//extern const uint8_t _binary_sample_vgm_start;
//extern const unsigned int _binary_sample_vgm_size;

//extern const uint8_t _binary_ball_nsf_start;
//extern const unsigned int _binary_ball_nsf_size;


//extern const uint8_t _binary_mario_nsf_start;
//extern const unsigned int _binary_mario_nsf_size;

//extern const uint8_t _binary_donk_nes_start;
//extern const unsigned int _binary_donk_nes_size;

//extern const uint8_t _binary_baloon_nes_start;
//extern const unsigned int _binary_baloon_nes_size;

//#define NSF_ROM   ((uint8_t*)&_binary_donk_nes_start)
//#define NSF_ROM_SIZE  ((unsigned int)&_binary_donk_nes_size)
#if 1

/*extern const uint8_t _binary_donk_nes_start;
extern const unsigned int _binary_donk_nes_size;

#define NSF_ROM   ((uint8_t*)&_binary_donk_nes_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_donk_nes_size)*/

/*extern const uint8_t _binary_baloon_nes_start;
extern const unsigned int _binary_baloon_nes_size;

#define NSF_ROM   ((uint8_t*)&_binary_baloon_nes_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_baloon_nes_size)*/


/*extern const uint8_t _binary_loonar_nes_start;
extern const unsigned int _binary_loonar_nes_size;

#define NSF_ROM   ((uint8_t*)&_binary_loonar_nes_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_loonar_nes_size)*/

extern const uint8_t _binary_smario_nes_start;
extern const unsigned int _binary_smario_nes_size;

#define NSF_ROM   ((uint8_t*)&_binary_smario_nes_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_smario_nes_size)

/*extern const uint8_t _binary_tanks_nes_start;
extern const unsigned int _binary_tanks_nes_size;

#define NSF_ROM   ((uint8_t*)&_binary_tanks_nes_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_tanks_nes_size)*/


Cartridge nes_cart(NSF_ROM, NSF_ROM_SIZE, false);
#else
extern const uint8_t _binary_goal3_nsf_start;
extern const unsigned int _binary_goal3_nsf_size;

#define NSF_ROM   ((uint8_t*)&_binary_goal3_nsf_start)
#define NSF_ROM_SIZE  ((unsigned int)&_binary_goal3_nsf_size)
Cartridge nes_cart(NSF_ROM, NSF_ROM_SIZE, true);
#endif

class AudioOutput : public IDmaRingBufferReadPos
{
public:
	virtual void playBuffer(int16_t* buffer, uint32_t size) = 0;
	virtual void stop() = 0;
	virtual void playBufferOnce(int16_t* buffer, uint32_t size) = 0;
	virtual uint32_t getReadPos() const = 0;
};

class I2S_AudioOutput : public AudioOutput
{
public:
    virtual void playBuffer(int16_t* buffer, uint32_t size) override
    {
        hi2s2.hdmatx->Init.Mode = DMA_CIRCULAR;
        HAL_DMA_Init(hi2s2.hdmatx);
        HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)buffer, size);
    };

    virtual void stop() override
    {
        HAL_I2S_DMAStop(&hi2s2);
    };

    virtual void playBufferOnce(int16_t* buffer, uint32_t size) override
    {
        hi2s2.hdmatx->Init.Mode = DMA_NORMAL;
        HAL_DMA_Init(hi2s2.hdmatx);
        HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)buffer, size);
    };

    virtual uint32_t getReadPos() const override
    {
        return AUDIO_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(hi2s2.hdmatx);
    };
};


VgmPlayer vgm_player;
//Apu2A03 apu;
Bus bus;
//Cpu6502 cpu;
I2S_AudioOutput audio_output;

class DWT_Stats {
public:
    // Call once before using any counters
    static void Init() {
        // Enable trace in CoreDebug (required for DWT writes to be accepted on some parts)
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

        // Try to enable all counters
        DWT->CYCCNT = 0;
        DWT->CPICNT = 0;
        DWT->EXCCNT = 0;
        DWT->SLEEPCNT = 0;
        DWT->LSUCNT = 0;
        DWT->FOLDCNT = 0;

        // Set control bits: CYCCNTENA plus the event counter enable bits (if supported)
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
                    | (1U << 8)   // CPICNTENA (implementation defined bit positions: CMSIS may not have named masks for these event counters)
                    | (1U << 9)   // EXCCNTENA
                    | (1U << 10)  // SLEEPCNTENA
                    | (1U << 11)  // LSUCNTENA
                    | (1U << 12); // FOLDCNTENA

        // Read back to see which bits actually stuck
        uint32_t ctrl = DWT->CTRL;
        printf("DWT_CTRL = 0x%08lX" ENDL, (unsigned long)ctrl);
        if (ctrl & DWT_CTRL_CYCCNTENA_Msk) printf("  CYCCNT enabled" ENDL); else printf("  CYCCNT NOT enabled" ENDL);
        // event-counter enable bits do not have universally-named CMSIS masks, so check by position:
        printf("  CPICNT bit %s" ENDL, (ctrl & (1U<<8)) ? "set" : "NOT set");
        printf("  EXCCNT bit %s" ENDL, (ctrl & (1U<<9)) ? "set" : "NOT set");
        printf("  SLEEPCNT bit %s" ENDL, (ctrl & (1U<<10)) ? "set" : "NOT set");
        printf("  LSUCNT bit %s" ENDL, (ctrl & (1U<<11)) ? "set" : "NOT set");
        printf("  FOLDCNT bit %s" ENDL, (ctrl & (1U<<12)) ? "set" : "NOT set");

        // Reset counters after check
        Reset();
    }

    // Reset all counters
    static void Reset() {
        DWT->CYCCNT   = 0;
        DWT->CPICNT   = 0;
        DWT->EXCCNT   = 0;
        DWT->SLEEPCNT = 0;
        DWT->LSUCNT   = 0;
        DWT->FOLDCNT  = 0;
    }

    // Enable counters (cycle counter is always on once enabled)
    static void Start() {
        // Enable all event counters
        // (bits 8-15 enable individual event counters)
        DWT->CTRL |=
            (1 << 0) |   // CYCCNTENA (bit0)
            (1 << 8) |   // CPICNT
            (1 << 9) |   // EXCCNT
            (1 << 10) |  // SLEEPCNT
            (1 << 11) |  // LSUCNT
            (1 << 12);   // FOLDCNT
    }

    // Read all counters (optional helper struct)
    struct Values {
        uint32_t cycles;
        uint32_t cpi;
        uint32_t exceptions;
        uint32_t sleep;
        uint32_t lsu;
        uint32_t fold;
    };

    static Values Read() {
        Values v;
        v.cycles     = DWT->CYCCNT;
        v.cpi        = DWT->CPICNT;
        v.exceptions = DWT->EXCCNT;
        v.sleep      = DWT->SLEEPCNT;
        v.lsu        = DWT->LSUCNT;
        v.fold       = DWT->FOLDCNT;
        return v;
    }

    // Print all counters with printf
    static void Print() {
        Values v = Read();

        printf("DWT statistics:" ENDL);
        printf("  CYCCNT    (cycles)       = %lu" ENDL, v.cycles);
        printf("  CPICNT    (CPI stalls)   = %lu" ENDL, v.cpi);
        printf("  EXCCNT    (exceptions)   = %lu" ENDL, v.exceptions);
        printf("  SLEEPCNT  (sleep cycles) = %lu" ENDL, v.sleep);
        printf("  LSUCNT    (LSU stalls)   = %lu" ENDL, v.lsu);
        printf("  FOLDCNT   (folded inst.) = %lu" ENDL, v.fold);
    }
};

#if 0
bool shell_cmd_test_dac(FILE *f, ShellCmd_t *cmd, const char *s) {
    HAL_StatusTypeDef res;
    int nsamples = 512;
    int16_t *dac_signal = (int16_t*)malloc(nsamples * sizeof(int16_t));
    if (dac_signal == NULL)
    {
        fprintf(f, "Heap alloc error" ENDL);
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
                               HAL_MAX_DELAY);
        if(res != HAL_OK) {
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
#endif

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

#if 0
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
#endif

#if 0
bool shell_emul_eeprom_format(FILE *f, ShellCmd_t *cmd, const char *s)
{
    HAL_FLASH_Unlock();
    EE_Status stat = EE_Format(EE_FORCED_ERASE);
    HAL_FLASH_Lock();
    if (stat == EE_OK)
    {
        fprintf(f, "EMUL_EEPROM format OK" ENDL);
    }
    else
    {
        fprintf(f, "EMUL_EEPROM format error %d" ENDL, stat);
        return false;
    }
    return true;
}

bool shell_emul_eeprom_read(FILE *f, ShellCmd_t *cmd, const char *s)
{
    uint32_t var_id = cmd->get_int_arg(s, 1);
    if (var_id == 0)
    {
        const uint32_t num_vars = 32;
        for (uint32_t id = 1; id < num_vars; id++)
        {
            uint32_t value;
            EE_Status stat = EE_ReadVariable32bits(id, &value);
            if (stat == EE_OK)
            {
                fprintf(f, "EMUL_EEPROM[0x%lX]=0x%08lX" ENDL, (unsigned long)id, (unsigned long)value);
            }
            else if (stat == EE_NO_DATA)
            {
            }
            else
            {
                fprintf(f, "EMUL_EEPROM[0x%lX] - read error %d" ENDL, (unsigned long)id, stat);
                return false;
            }
        }
    }
    else
    {
        uint32_t value;
        EE_Status stat = EE_ReadVariable32bits(var_id, &value);
        if (stat == EE_OK)
        {
            fprintf(f, "EMUL_EEPROM[0x%lX]=0x%08lX" ENDL, var_id, value);
        }
        else
        {
            fprintf(f, "EMUL_EEPROM[0x%lX] - read error %d" ENDL, var_id, stat);
            return false;
        }
    }
    return true;
}

bool shell_emul_eeprom_write(FILE *f, ShellCmd_t *cmd, const char *s)
{
    uint32_t var_id = cmd->get_int_arg(s, 1);
    uint32_t value = cmd->get_int_arg(s, 2);
    HAL_FLASH_Unlock();
    EE_Status stat = EE_WriteVariable32bits(var_id, value);
    if (stat == EE_CLEANUP_REQUIRED)
    {
        fprintf(f, "EMUL_EEPROM cleanup...");
        stat = EE_CleanUp();
        if (stat == EE_OK)
        {
            fprintf(f, "OK" ENDL);
        }
        else
        {
            fprintf(f, "error %d" ENDL, stat);
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
    if (stat == EE_OK)
    {
        fprintf(f, "EMUL_EEPROM[0x%lX]=0x%08lX - OK" ENDL, var_id, value);
    }
    else
    {
        fprintf(f, "EMUL_EEPROM[0x%lX] - write error %d" ENDL, var_id, stat);
        return false;
    }
    return true;
}
#endif

#if 0
const char* ascii_art_digits[11][7] = {
    {
        " ▗▄▖ ",
        " █▀█ ",
        "▐▌ ▐▌",
        "▐▌█▐▌",
        "▐▌ ▐▌",
        " █▄█ ",
        " ▝▀▘ ",
    },
    {
        " ▗▄  ",
        " ▛█  ",
        "  █  ",
        "  █  ",
        "  █  ",
        "▗▄█▄▖",
        "▝▀▀▀▘",
    },
    {
        " ▄▄▖ ",
        "▐▀▀█▖",
        "   ▐▌",
        "  ▗▛ ",
        " ▗▛  ",
        "▗█▄▄▖",
        "▝▀▀▀▘",
    },
    {
        " ▄▄▖ ",
        "▐▀▀█▖",
        "   ▟▌",
        " ▐██ ",
        "   ▜▌",
        "▐▄▄█▘",
        " ▀▀▘ ",
    },
    {
        "  ▗▄ ",
        "  ▟█ ",
        " ▐▘█ ",
        "▗▛ █ ",
        "▐███▌",
        "   █ ",
        "   ▀ ",
    },
    {
        "▗▄▄▄ ",
        "▐▛▀▀ ",
        "▐▙▄▖ ",
        "▐▀▀█▖",
        "   ▐▌",
        "▐▄▄█▘",
        " ▀▀▘ ",
    },
    {
        " ▗▄▖ ",
        " █▀▜ ",
        "▐▌▄▖ ",
        "▐█▀█▖",
        "▐▌ ▐▌",
        "▝█▄█▘",
        " ▝▀▘ ",
    },
    {
        "▗▄▄▄▖",
        "▝▀▀█▌",
        "  ▗█ ",
        "  ▐▌ ",
        "  █  ",
        " ▐▌  ",
        " ▀   ",
    },
    {
        " ▗▄▖ ",
        "▗█▀█▖",
        "▐▙ ▟▌",
        " ███ ",
        "▐▛ ▜▌",
        "▝█▄█▘",
        " ▝▀▘ ",
    },
    {
        " ▗▄▖ ",
        "▗█▀█▖",
        "▐▌ ▐▌",
        "▝█▄█▌",
        " ▝▀▐▌",
        " ▙▄█ ",
        " ▝▀▘ ",
    },
    {
        "     ",
        "     ",
        "     ",
        "     ",
        "     ",
        "     ",
        "     ",
    }
};

void print_ascii_art(FILE *f, const char* text)
{
    for (int row = 0; row < 7; row++)
    {
        const char *s = text;
        while (*s)
        {
            char ch = *s;
            int d;
            if ((ch >= '0') && (ch <= '9'))
            {
                d = ch - '0';
            }
            else
            {
                d = 10;
            }
            fprintf(f, "%s ", ascii_art_digits[d][row]);
            s++;
        }
        fprintf(f, ENDL);
    }
}

void print_ascii_art_number(FILE *f, int num)
{
    if (num < 0)
    {
        num = 0;
    }
    else if (num > 99999999)
    {
        num = 99999999;
    }

    const int EMPTY_DIGIT = 10;
    const int NUM_DIGITS = 8;
    int digits[NUM_DIGITS];
    for (int i = 0; i < NUM_DIGITS; i++)
    {
        digits[i] = EMPTY_DIGIT;
    }

    int idx = 7;
    while (num > 0)
    {
        digits[idx] = num % 10;
        num /= 10;
        idx--;
    }

    for (int row = 0; row < 7; row++)
    {
        for (int d = 0; d < 8; d++)
        {
            fprintf(f, "%s ", ascii_art_digits[digits[d]][row]);
        }
        fprintf(f, ENDL);
    }
}

void uint_to_grouped_str(unsigned int value, char *out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", value);

    int len = strlen(buf);
    int out_idx = 9; // last char index for 10-char output
    out[10] = '\0';

    int digit_count = 0;
    for (int i = len - 1; i >= 0; --i) {
        if (digit_count > 0 && digit_count % 3 == 0) {
            out[out_idx--] = ' ';
        }
        out[out_idx--] = buf[i];
        digit_count++;
    }
    // Fill remaining with spaces for right alignment
    while (out_idx >= 0) {
        out[out_idx--] = ' ';
    }
}

bool shell_pulse_counter_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, BG_BLUE FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_HIDE_CURSOR);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (true)
    {
        int c = fgetc(f);
        if (c =='q')
        {
            break;
        }
        uint32_t cnt = __HAL_TIM_GET_COUNTER(&htim2);
        //fprintf(f, "Pulse counter: %lu" ENDL, (unsigned long)cnt);
        char buf[11];
        uint_to_grouped_str(cnt, buf);
        fprintf(f, VT100_CURSOR_HOME);
        print_ascii_art(f, buf);
        osDelay(200);
    }
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    return true;
}

bool shell_freq_measure(FILE *f, ShellCmd_t *cmd, const char *s)
{
    uint32_t duration_ms = cmd->get_int_arg(s, 1);
    fprintf(f, BG_BLUE FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_HIDE_CURSOR);
    if (duration_ms == 0)
    {
        duration_ms = 1000;
    }
    __HAL_TIM_ENABLE(&htim3);
    while (true)
    {
        int c = fgetc(f);
        if (c =='q')
        {
            break;
        }
        
        __disable_irq();
        __HAL_TIM_SET_COUNTER(&htim2, 0);
        for (uint32_t i = 0; i < 1000; i++)
        {
            __HAL_TIM_SET_COUNTER(&htim3, 0);
            while ((__HAL_TIM_GET_COUNTER(&htim3)) < duration_ms); 
        }

        //osDelay(duration_ms);
        //HAL_Delay(duration_ms);

        uint32_t cnt = __HAL_TIM_GET_COUNTER(&htim2);
        __enable_irq();
        int freq = ((float)cnt) / ((float)duration_ms) * 1000.0f;
        char buf[11];
        uint_to_grouped_str(freq, buf);
        fprintf(f, VT100_CURSOR_HOME);
        print_ascii_art(f, buf);
        //fprintf(f, "Frequency: %d Hz, cnt=%lu" ENDL, freq, cnt);
    }
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    return true;
}

bool shell_asc(FILE *f, ShellCmd_t *cmd, const char *s)
{
    const char* arg = cmd->get_str_arg(s, 1);
    print_ascii_art(f, arg);
    return true;
}
#endif

bool shell_cmd_play_nes_chiptune(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, "Playing nes chiptune..." ENDL);
    //audio_output.playBuffer(apu.audio_buffer, AUDIO_BUFFER_SIZE);

    /*VgmPlayer::Status status = vgm_player.play(&_binary_sample_vgm_start,
                                              (size_t)&_binary_sample_vgm_size,
                                              apu,
                                              stdout);*/
    audio_output.stop();                                  
    //(void)status;

    return true;
}

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));
    /*shell.add_command(ShellCmd_t("heap", "", shell_heap_stat));
    shell.add_command(ShellCmd_t("eeformat", "", shell_emul_eeprom_format));
    shell.add_command(ShellCmd_t("eerd", "", shell_emul_eeprom_read));
    shell.add_command(ShellCmd_t("eewr", "", shell_emul_eeprom_write));*/
    //shell.add_command(ShellCmd_t("pcnt", "", shell_pulse_counter_stat));
    //shell.add_command(ShellCmd_t("freq", "", shell_freq_measure));
    //shell.add_command(ShellCmd_t("asc", "", shell_asc));
#if (EN_AUDIO_SHELL_CMDS)    
    //shell.add_command(ShellCmd_t("dac_test", "Test DAC", shell_cmd_test_dac));
    shell.add_command(ShellCmd_t("plnes", "Play nes chiptune", shell_cmd_play_nes_chiptune));
#endif
    shell.set_device(device);
    shell.print_prompt();
}

extern "C" void task_nes_emu_main(void *argument);
osThreadId_t task_handle_nes_emu_main;

extern "C" void task_nes_emu_main(void *argument)
{

    audio_output.playBuffer(bus.cpu.apu.audio_buffer, AUDIO_BUFFER_SIZE);
    while (1)
    {
        //__disable_irq();
        uint32_t time_start = DWT->CYCCNT;
        uint32_t elapsed;
        uint32_t frame __attribute__((used))= 0;
        for (frame = 0; frame < 100; frame++)
        {
            bus.clock();

            button1.update();
            button2.update();
            //keypad.update();
            bus.controller = 0x00;
            bus.controller |= (button1.is_pressed() ? Bus::CONTROLLER::Start : 0x00);
            bus.controller |= (button2.is_pressed() ? Bus::CONTROLLER::A : 0x00);
            /*bus.controller |= (keypad.button1().is_pressed() ? Bus::CONTROLLER::Left : 0x00);
            bus.controller |= (keypad.button2().is_pressed() ? Bus::CONTROLLER::Right : 0x00);
            bus.controller |= (keypad.button4().is_pressed() ? Bus::CONTROLLER::A : 0x00);

            if (keypad.button3().is_pressed_event())
            {
            }*/
        };
#if 1
        //DWT_Stats::Print();
        elapsed = DWT->CYCCNT - time_start;
        //__enable_irq();
        uint32_t cycles_per_frame = elapsed / frame;
        uint32_t fps = (SystemCoreClock + cycles_per_frame / 2) / cycles_per_frame;
        uint32_t cpu_cycles_per_sec = fps * 29928; // NTSC CPU cycles per frame
        uint32_t apu_cycles_per_sec = fps * 29928 / 2;
        uint32_t apu_instructions_per_sec = bus.cpu.total_instructions * 1000 / elapsed;
        bus.cpu.total_cycles = 0;
        bus.cpu.apu.total_cycles = 0;
        bus.cpu.total_instructions = 0;
        printf("NES Emu: %lu fps, %lu cpu_cycles/sec, %lu apu_cycles/sec, %lu instr/sec elapsed=%lu ms" ENDL,
               (unsigned long)fps,
               (unsigned long)cpu_cycles_per_sec,
               (unsigned long)apu_cycles_per_sec,
               (unsigned long)apu_instructions_per_sec,
               (unsigned long)elapsed);
        bus.cpu.cpu_prefetcher.printStats();
        bus.cpu.cpu_prefetcher.resetStats();
        printf("Reads: %lu, ram_reads: %lu, Writes: %lu, ram_writes: %lu" ENDL,
               (unsigned long)bus.total_reads,
               (unsigned long)bus.ram_reads,
               (unsigned long)bus.total_writes,
               (unsigned long)bus.ram_writes);
        printf("Cycles per frame: %lu" ENDL, (unsigned long)cycles_per_frame);
        //DWT_Stats::Reset();
        //osDelay(10);
#endif
    }

}

extern "C" void task_user_input(void *argument);
osThreadId_t task_handle_user_input;

extern "C" void task_user_input(void *argument)
{

    audio_output.playBuffer(bus.cpu.apu.audio_buffer, AUDIO_BUFFER_SIZE);
    while (1)
    {
        bus.cpu.apu.clock(10);
    }

    /*GpioPinPortB_t<10> keypad_key2;
    GpioPinPortB_t<2> keypad_key1;
    GpioPinPortB_t<0> keypad_key4;
    GpioPinPortA_t<7> keypad_key3;

    Keypad_4x1 keypad(&keypad_key1, &keypad_key2, &keypad_key3, &keypad_key4);
    keypad.init();

    uint32_t button_update_divider_cnt = 0;
    for(;;)
    {
        shell.run();

        if (button_update_divider_cnt % 4 == 0)
        {
            button_update_divider_cnt = 0;
            button1.update();
            button2.update();
            keypad.update();

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
        }
        osDelay(50);
        button_update_divider_cnt++;
    }*/
}

#if 0
void emul_eeprom_init()
{
    HAL_FLASH_Unlock();
    EE_Status stat = EE_Init(EE_CONDITIONAL_ERASE);
    HAL_FLASH_Lock();
    if (stat == EE_OK)
    {
        printf("EMUL_EEPROM init - OK" ENDL);
    }
    else
    {
        printf("EMUL_EEPROM init - error %d" ENDL, stat);
        return;
    }
}
#endif
void pulse_counter_init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    // Disable timer before config
    __HAL_TIM_DISABLE(&htim2);

    // Configure CH1 as input (TI1)
    TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;
    TIM2->CCMR1 |= TIM_CCMR1_CC1S_0;

    // Select rising edge
    TIM2->CCER &= ~TIM_CCER_CC1P;

    // Trigger = TI1FP1 (101)
    TIM2->SMCR &= ~TIM_SMCR_TS;
    TIM2->SMCR |= TIM_SMCR_TS_2 | TIM_SMCR_TS_0;

    // Slave mode = external clock mode 1 (SMS = 111)
    TIM2->SMCR &= ~TIM_SMCR_SMS;
    TIM2->SMCR |= TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1 | TIM_SMCR_SMS_0;

    // Enable counter
    __HAL_TIM_ENABLE(&htim2);
}

void apuInit()
{
    //bus.cpu.apu.connectBus(&bus);
    //bus.cpu.apu.connectCPU(&bus.cpu);

    array<uint8_t, 20> initialRegisters = {
        0x30, 0x08, 0x00, 0x00, // Pulse 1
        0x30, 0x08, 0x00, 0x00, // Pulse 2
        0x80, 0x00, 0x00, 0x00, // Triangle
        0x30, 0x00, 0x00, 0x00, // Noise
        0x00, 0x00, 0x00, 0x00 // DMC
    };
    for (size_t i = 0; i < initialRegisters.size(); i++) {
        bus.cpu.apu.cpuWrite(0x4000 + i, initialRegisters[i]);
    }
    // Enable all channels
    bus.cpu.apu.cpuWrite(0x4015, 0x0F);
    bus.cpu.apu.cpuWrite(0x4017, 0x40);
}   

const uint32_t NSF_HEADER_SIZE = 0x80;

extern TIM_HandleTypeDef htim1;

void My_LCD_TransferComplete(DMA_HandleTypeDef *hdma)
{
    // 1. Stop the Timer immediately!
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);

    // 2. Disable DMA Request
    __HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_UPDATE);

    // 3. Deselect Chip (Optional, but good practice)
    //HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

extern "C" void init()
{
#if 1
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
#endif
    //apu.connectDma(&audio_output);
    //keypad.init();

    /*ST7735_Init();
    ST7735_FillScreen(ST7735_GREEN);
    St7735_Vt100_t lcd_vt100_terminal;
    lcd_vt100_terminal.init(Font_7x10);
    FILE *flcd = lcd_vt100_terminal.fopen();
    init_shell(flcd);
    fprintf(flcd, "Hello from %s (FreeRTOS)!" ENDL, MCU_NAME_STR);
    ST7735_SetAddressWindow(0, 0, 127, 159);
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);*/
    //ili9341_Init();
    LCD_WR_MODE_GPIO();
    ILI9341_Init();
    HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_RESET);
    ILI9341_SetAddressWindow(32, 0, /*127*/32 + 255, 239);
    //ILI9341_FillScreen(ILI9341_YELLOW);
    //ili9341_DisplayOff();
    //ili9341_DisplayOn();
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);

    for (int i = 0; i < SCANLINE_SIZE; i++)
    {
        //bus.ppu.display_buffer[0][i] = 0x142D;
        //bus.ppu.display_buffer[1][i] = 0x142D;
        bus.ppu.display_buffer[0][i] = 0x00FF;
        bus.ppu.display_buffer[1][i] = 0x00FF;
    }

    //ili9341_FillScreen(ILI9341_GREEN);
    //Ili9341_Vt100_t lcd_vt100_terminal;
    //lcd_vt100_terminal.init(Font_7x10);
  
    /*hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&hspi1);*/
    LCD_WR_MODE_PWM();
    /* 1. Set the Address of the GPIO B Output Data Register */
    /* Note: We cast to uint8_t* to enforce byte-width access */
    uint32_t destination_address = (uint32_t)&GPIOB->ODR;

    /* 2. Configure the DMA Source and Length */
    /* Note: Use the HAL or LL macro depending on your init */
    /* Example using HAL_DMA_Start (But we need it linked to TIM1) */

    // The standard HAL_TIM_PWM_Start_DMA won't work perfectly here 
    // because it expects to transfer data TO the CCR register (Duty Cycle), 
    // not to a GPIO.
//ppu.display_buffer[ppu.write_buf_idx], 256 * SCANLINES_PER_BUFFER * 2
    // You must manually link the DMA to the External destination:

    htim1.hdma[TIM_DMA_ID_UPDATE]->XferCpltCallback = My_LCD_TransferComplete;
    htim1.hdma[TIM_DMA_ID_UPDATE]->Init.Mode = DMA_CIRCULAR;
    HAL_DMA_Init(htim1.hdma[TIM_DMA_ID_UPDATE]);
    __HAL_TIM_MOE_ENABLE(&htim1); // Main Output Enable for TIM1
    bus.ppu.write_buf_idx = 0;
    bus.ppu.x = 0;
    /*for (int i = 0; i < 8; i++)
    {
        bus.ppu.display_buffer[bus.ppu.write_buf_idx][i] = 0x142D;
    }*/
#if 0
    HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_UPDATE], (uint32_t)bus.ppu.display_buffer[0], destination_address, SCANLINE_SIZE * SCANLINES_PER_BUFFER * 4);

    // 3. Enable the TIM1 Update DMA Request
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);

    // 4. Start the PWM (This starts the clock and triggers the DMA)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
#endif
    //while (1);
    // ... Wait for transfer complete ...

    // 5. Stop everything
    //HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    //__HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_UPDATE);

    /* Now check Logic Analyzer. 
    If you see a square wave now, your Timer/GPIO settings are correct, 
    and the issue lies in your DMA configuration (likely an error flag triggering). */

    bus.cpu.apu.connectDma(&audio_output);
    bus.cpu.connectBus(&bus);
    bus.insertCartridge(&nes_cart);
    bus.reset();
    DWT_Stats::Init();
    DWT_Stats::Start();

    /*hi2s2.hdmatx->Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(hi2s2.hdmatx);
    HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)buffer, size);
    HAL_SPI_Transmit_DMA(&hspi3, (uint8_t*)buffer, size);*/

    /*osThreadAttr_t defaultTask_attributes = { };
    defaultTask_attributes.name = "task_user_input";
    defaultTask_attributes.stack_size = 256 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;
    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);*/
#if 1
    osThreadAttr_t nesEmuMainTask_attributes = { };
    nesEmuMainTask_attributes.name = "task_nes_emu_main";
    nesEmuMainTask_attributes.stack_size = 256 * 4;
    nesEmuMainTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_nes_emu_main = osThreadNew(task_nes_emu_main, NULL, &nesEmuMainTask_attributes);

#else
    uint32_t loadAddr = ((uint16_t*)NSF_ROM)[4];
    uint32_t initFuncAddr = ((uint16_t*)NSF_ROM)[5];
    uint32_t playFuncAddr = ((uint16_t*)NSF_ROM)[6];
    uint32_t songCount = ((uint8_t*)NSF_ROM)[6];
    uint32_t firstSong = ((uint8_t*)NSF_ROM)[7];
    uint32_t songNo = firstSong;
    uint32_t periodInUsecsNtsc = ((uint16_t*)NSF_ROM)[0x37];
    uint32_t periodInUsecsPal = ((uint16_t*)NSF_ROM)[0x3C];
    bool isDualNtscPal = (((uint8_t*)NSF_ROM)[0x7A] & 0x02) != 0;
    bool isPal = (((uint8_t*)NSF_ROM)[0x7A] & 0x01) != 0;
    bool isNtsc = isDualNtscPal || !isPal;
    uint32_t periodInUsecs = isNtsc ? periodInUsecsNtsc : periodInUsecsPal;
    //nes_cart.addMemory(loadAddr, ((uint8_t*)NSF_ROM) + NSF_HEADER_SIZE, NSF_ROM_SIZE - NSF_HEADER_SIZE);
    audio_output.playBuffer(bus.cpu.apu.audio_buffer, AUDIO_BUFFER_SIZE);
    uint32_t apuClksPerFrame = (uint64_t)894886 * periodInUsecs / 1000000;  // /*18623*50/60=15519, 14914*/
    bool loadNewSong = true;

    while (true)
    {
        if (loadNewSong)
        {
            loadNewSong = false;
            bus.reset();
            apuInit();
            //bus.cpu.reset();
            bus.cpu.PC = initFuncAddr;
            bus.cpu.A = songNo - 1;
            bus.cpu.X = isNtsc ? 0 : 1;
            bus.cpu.clock(100000);
        }

        bus.cpu.PC = playFuncAddr;
        bus.cpu.SP = 0xFD;
        bus.cpu.clock(100000);
        bus.cpu.apu.clock(apuClksPerFrame);

        button1.update();
        button2.update();
        //keypad.update();

        if (button1.is_pressed_event())
        {
            songNo++;
            if (songNo > songCount)
            {
                songNo = firstSong;
            }
            loadNewSong = true;
        }

        if (button2.is_pressed_event())
        {
            if (songNo == firstSong)
            {
                songNo = songCount;
            }
            else
            {
                songNo--;
            }
            loadNewSong = true;
        }
    }

    /*VgmPlayer::Status status = vgm_player.play(&_binary_sample_vgm_start,
                                              (size_t)&_binary_sample_vgm_size,
                                              apu,
                                              stdout);
    (void)status;
    audio_output.stop();*/

    //emul_eeprom_init();

    init_shell();  
    pulse_counter_init();
#endif

}
