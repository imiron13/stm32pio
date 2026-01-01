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
#include <analog_stick.h>

using namespace std;

#ifndef BOARD_SUPPORT_SD_CARD
#define BOARD_SUPPORT_SD_CARD                                   (0)
#endif

#ifndef BOARD_SUPPORT_AUDIO
#define BOARD_SUPPORT_AUDIO                                     (0)
#endif

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

Shell_t shell;

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
        //HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)buffer, size);
        //LL_DMA_SetPeriphAddress(DMAx, CH, (uint32_t)&SPIx->DR + 1);
        HAL_DMA_Start(
            hi2s2.hdmatx,
            (uint32_t)buffer,              // uint8_t*
            (uint32_t)&SPI2->DR + 1,        // HIGH BYTE
            size                            // number of bytes
        );
        SET_BIT(hi2s2.Instance->CR2, SPI_CR2_TXDMAEN);
        __HAL_I2S_ENABLE(&hi2s2);
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
        return AUDIO_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(hi2s2.hdmatx) / 2;
    };
};


VgmPlayer vgm_player;
Bus bus;
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
    shell.set_device(device);
    shell.print_prompt();
}

extern "C" void task_nes_emu_main(void *argument);
osThreadId_t task_handle_nes_emu_main;

extern "C" void task_nes_emu_main(void *argument)
{
    // --- Calibration (Optional) ---
    // Perform ADC calibration for better accuracy on G4 series
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

    // --- Driver Configuration ---
    AnalogStickConfig joyConfig;
    
    // Configure X Axis (Using PB14 -> ADC1 Channel 5)
    joyConfig.hAdcX = &hadc1;
    joyConfig.channelX = ADC_CHANNEL_5;

    // Configure Y Axis (Using PB2 -> ADC2 Channel 12)
    // Note: If PB2 is on ADC1, simply change this to &hadc1 and the correct channel
    joyConfig.hAdcY = &hadc2; 
    joyConfig.channelY = ADC_CHANNEL_12; 

    // Configure Button (PB6)
    joyConfig.btnPort = GPIOB;
    joyConfig.btnPin = GPIO_PIN_6;
    joyConfig.deadzone = 150; // Slight deadzone to prevent drift

    // Instantiate Driver
    AnalogStick joystick(joyConfig);

    audio_output.playBuffer(bus.cpu.apu.audio_buffer, AUDIO_BUFFER_SIZE*2);
    while (1)
    {
        __disable_irq();
        uint32_t time_start = DWT->CYCCNT;
        uint32_t elapsed;
        uint32_t frame __attribute__((used))= 0;
        for (frame = 0; frame < 100; frame++)
        {
            bus.clock();

            button1.update();
            button2.update();

            bus.controller = 0x00;
            bus.controller |= (button1.is_pressed() ? Bus::CONTROLLER::Start : 0x00);
            bus.controller |= (button2.is_pressed() ? Bus::CONTROLLER::A : 0x00);
            joystick.update();
            float x = joystick.getX(); // Returns -1.0 to 1.0
            float y = joystick.getY(); // Returns -1.0 to 1.0
            if (joystick.isPressed())
            {
                bus.controller |= Bus::CONTROLLER::B;
            }
            if (x < -0.5f)
            {
                bus.controller |= Bus::CONTROLLER::Right;
            }
            else if (x > 0.5f)
            {
                bus.controller |= Bus::CONTROLLER::Left;
            }
            if (y < -0.5f)
            {
                bus.controller |= Bus::CONTROLLER::A;
            }
            else if (y > 0.5f)
            {
                bus.controller |= Bus::CONTROLLER::Down;
            }
        };
#if 1
        //DWT_Stats::Print();
        elapsed = DWT->CYCCNT - time_start;
        __enable_irq();
        uint32_t cycles_per_frame = elapsed / frame;
        uint32_t fps = (SystemCoreClock + cycles_per_frame / 2) / cycles_per_frame;
        uint32_t cpu_cycles_per_sec = fps /** 29928*/ * ((114 + 113 + 114) * 80 + 2501) /*29781*/; // NTSC CPU cycles per frame
        uint32_t apu_cycles_per_sec = fps * 29928 / 2;
        uint32_t apu_events_per_frame = (bus.cpu.apu.apu_events + 50) / 100;
        uint32_t apu_instructions_per_sec = bus.cpu.total_instructions * 1000 / elapsed;
        bus.cpu.total_cycles = 0;
        bus.cpu.apu.total_cycles = 0;
        bus.cpu.total_instructions = 0;
        printf("NES Emu: %lu fps, %lu cpu_cycles/sec, %lu apu_cycles/sec, %lu apu_ev/fr %lu instr/sec elapsed=%lu ms" ENDL,
               (unsigned long)fps,
               (unsigned long)cpu_cycles_per_sec,
               (unsigned long)apu_cycles_per_sec,
               (unsigned long)apu_events_per_frame,
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
        bus.cpu.apu.apu_events = 0;
        //DWT_Stats::Reset();
        //osDelay(10);
#endif
    }

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

// Import the linker symbols we defined
extern uint32_t _siccmram;  // Source (Flash)
extern uint32_t _sccmram;   // Destination (CCM RAM)
extern uint32_t _eccmram;   // End (CCM RAM)

void Copy_CCMRAM(void)
{
    uint32_t *src = &_siccmram;
    uint32_t *dst = &_sccmram;
    uint32_t *end = &_eccmram;

    // Copy data from Flash to CCM RAM
    while (dst < end) {
        *dst++ = *src++;
    }
}

extern "C" void init()
{
    Copy_CCMRAM();

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

    LCD_WR_MODE_GPIO();
    ILI9341_Init();

    bus.cpu.apu.connectDma(&audio_output);
    bus.cpu.connectBus(&bus);
    bus.insertCartridge(&nes_cart);
    bus.reset();
    DWT_Stats::Init();
    DWT_Stats::Start();

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

    init_shell();  
#endif

}
