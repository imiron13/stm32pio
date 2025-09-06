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
extern "C" {
#include <eeprom_emul.h>
}

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
#define EN_AUDIO_SHELL_CMDS                                 (1)
#define EN_TETRIS                                           (1)

extern UART_HandleTypeDef huart1;
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern TIM_HandleTypeDef htim2;
Shell_t shell;

#if (EN_AUDIO_SHELL_CMDS == 1)
#include <math.h>

#define PI 3.14159265358979323846
#define TAU (2.0 * PI)

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

bool shell_pulse_counter_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    while (true)
    {
        int c = fgetc(f);
        if (c =='q')
        {
            break;
        }
        uint32_t cnt = __HAL_TIM_GET_COUNTER(&htim2);
        fprintf(f, "Pulse counter: %lu" ENDL, (unsigned long)cnt);
        osDelay(500);
    }
    return true;
}

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));
    shell.add_command(ShellCmd_t("heap", "", shell_heap_stat));
    shell.add_command(ShellCmd_t("eeformat", "", shell_emul_eeprom_format));
    shell.add_command(ShellCmd_t("eerd", "", shell_emul_eeprom_read));
    shell.add_command(ShellCmd_t("eewr", "", shell_emul_eeprom_write));
    shell.add_command(ShellCmd_t("pcnt", "", shell_pulse_counter_stat));
#if (EN_AUDIO_SHELL_CMDS)    
    shell.add_command(ShellCmd_t("dac_test", "Test DAC", shell_cmd_test_dac));
#endif
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
    }       
}

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

    emul_eeprom_init();

    init_shell();  
    pulse_counter_init();

    osThreadAttr_t defaultTask_attributes = { };
    defaultTask_attributes.name = "task_user_input";
    defaultTask_attributes.stack_size = 256 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);
}
