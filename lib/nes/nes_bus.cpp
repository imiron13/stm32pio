#include "nes_apu.h"
#include "nes_bus.h"
#include "nes_cpu.h"
#include "intrinsics.h"
#include <cstdint>
#include <cstring>

#include <hal_wrapper_stm32.h>
#include "st7735.h"

#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_spi.h"

using namespace std;

#define IRAM_ATTR
#define FRAMESKIP

Bus::Bus()
{
    memset(RAM, 0, sizeof(RAM));
    cpu.connectBus(this);
    cpu.apu.connectBus(this);
    ppu.connectBus(this);
}

Bus::~Bus()
{
}

IRAM_ATTR void Bus::cpuWriteNonRam(uint16_t addr, uint8_t data)
{
    if (cart->cpuWrite(addr, data)) {}
    else if ((addr & 0xE000) == 0x2000)
    {
        ppu.cpuWrite(addr, data);
    }
    else if ((addr & 0xF000) == 0x4000 && (addr <= 0x4013 || addr == 0x4015 || addr == 0x4017))
    {
        cpu.apuWrite(addr, data);
    }
    else if (addr == 0x4014)
    {
        cpu.OAM_DMA(data);
    }
    else if (addr == 0x4016)
    {
        controller_strobe = data & 1;
        if (controller_strobe)
        {
            controller_state = controller;
        }
    }
}

IRAM_ATTR uint8_t Bus::cpuReadNonRam(uint16_t addr)
{
    uint8_t data = 0x00;
    if (cart->cpuRead(addr, data)) {}
    else if ((addr & 0xE000) == 0x2000)
    {
        data = ppu.cpuRead(addr);
    }
    else if (addr == 0x4016)
    {
        uint8_t value = controller_state & 1;
        if (!controller_strobe)
            controller_state >>= 1;
        data = value | 0x40;
    }
    return data;
}

void Bus::cpuReadBlock(uint16_t addr, uint32_t size, uint8_t* data)
{
    if (cart->cpuReadBlock(addr, size, data)) {}
    else
    {
        for (uint32_t i = 0; i < size; i++)
        {
            data[i] = cpuRead(addr + i);
        }
    }
}

extern TIM_HandleTypeDef htim1;

void spi_dma_init(void *data, size_t size)
{
#if 0    
    // DMAMUX → SPI1_TX
    LL_SPI_SetDataWidth(SPI1, LL_SPI_DATAWIDTH_16BIT);
    LL_DMAMUX_SetRequestID(DMAMUX1, LL_DMAMUX_CHANNEL_1, LL_DMAMUX_REQ_SPI1_TX);

    // DMA channel config (never changed)
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)&SPI1->DR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, size/2);

    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_HIGH);

    // No interrupts
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_2);

    // SPI config done elsewhere
    LL_SPI_Enable(SPI1);
    LL_SPI_EnableDMAReq_TX(SPI1);
#endif
    /* 1. Set the Address of the GPIO B Output Data Register */
    /* Note: We cast to uint8_t* to enforce byte-width access */
    uint32_t destination_address = (uint32_t)&GPIOA->ODR;

    /* 2. Configure the DMA Source and Length */
    /* Note: Use the HAL or LL macro depending on your init */
    /* Example using HAL_DMA_Start (But we need it linked to TIM1) */

    // The standard HAL_TIM_PWM_Start_DMA won't work perfectly here 
    // because it expects to transfer data TO the CCR register (Duty Cycle), 
    // not to a GPIO.

    // You must manually link the DMA to the External destination:
    HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_UPDATE], (uint32_t)data, destination_address, size);

    // 3. Enable the TIM1 Update DMA Request
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);

    // 4. Start the PWM (This starts the clock and triggers the DMA)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    // ... Wait for transfer complete ...

    // 5. Stop everything
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
__HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_UPDATE);
}

#include "hal_wrapper_stm32.h"
extern TIM_HandleTypeDef htim1;

/* Returns the number of bytes the CPU can safely write before overwriting 
   data that the DMA has not yet sent to the display. */
inline uint32_t Get_DMA_Remain()
{
    return __HAL_DMA_GET_COUNTER(htim1.hdma[TIM_DMA_ID_UPDATE]);
}

uint32_t Get_DMA_Read_Ptr(uint32_t buffer_size)
{
    // 1. Get current DMA counter (Remaining items)
    // Note: Use the Handle for the specific Channel you are using (TIM_UP)
    uint32_t dma_remaining = __HAL_DMA_GET_COUNTER(htim1.hdma[TIM_DMA_ID_UPDATE]);
    
    // 2. Convert to absolute Index (0 to buffer_size - 1)
    uint32_t dma_read_index = buffer_size - dma_remaining;
    return dma_read_index;
}

uint32_t Get_DMA_Free_Space(uint32_t cpu_write_index, uint32_t buffer_size)
{
    // 1. Get current DMA counter (Remaining items)
    // Note: Use the Handle for the specific Channel you are using (TIM_UP)
    uint32_t dma_remaining = __HAL_DMA_GET_COUNTER(htim1.hdma[TIM_DMA_ID_UPDATE]);
    
    // 2. Convert to absolute Index (0 to buffer_size - 1)
    uint32_t dma_read_index = buffer_size - dma_remaining;

    // 3. Calculate distance
    // Logic: (Read - Write + Size) % Size
    // If Read > Write: Distance is the gap between them.
    // If Write > Read: Distance is space to end + space from start to Read.
    
    uint32_t free_space = 0;
    if (dma_read_index >= cpu_write_index) {
        free_space = dma_read_index - cpu_write_index;
    } else {
        free_space = (buffer_size - cpu_write_index) + dma_read_index;
    }

    // 4. Safety Margin (Optional but Recommended)
    // Keep at least 1-4 bytes gap so pointers don't overlap completely
    if (free_space > 0) free_space--; 
    
    return free_space;
}

#include "ili9341.h"

#define DISPLAY_PAUSE()   (TIM1->CR1 &= ~TIM_CR1_CEN)
#define DISPLAY_RESUME()  (TIM1->CR1 |= TIM_CR1_CEN)


/* Add this to your Initialization or Burst Macro */
#define CONFIGURE_BURST_MODE(pixels) do { \
    /* ... (Your existing setup) ... */ \
    TIM1->RCR = (pixels) - 1; \
    TIM1->CCR2 = 1; \
    TIM1->DIER |= TIM_DIER_CC2DE; \
    TIM1->DIER &= ~TIM_DIER_UDE; \
    TIM1->CR1 |= TIM_CR1_OPM; \
    \
    /* LOAD SHADOW REGISTERS */ \
    TIM1->EGR = TIM_EGR_UG; \
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE); \
    \
    /* THE FIX: ENABLE OUTPUTS MANUALLY */ \
    /* 1. Enable Channel 1 (CC1E) */ \
    TIM1->CCER |= TIM_CCER_CC1E; \
    /* 2. Enable Main Output (MOE) - Critical for TIM1/TIM8 */ \
    TIM1->BDTR |= TIM_BDTR_MOE; \
} while(0)

void set_window()
{
    LCD_WR_MODE_GPIO();
    ILI9341_SetAddressWindow(32, 0, 32 + 255, 239);
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    LCD_WR_MODE_PWM();
}

void setx(uint32_t x0, uint32_t x1)
{
    LCD_WR_MODE_GPIO();
    ILI9341_WriteCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // write to RAM
    ILI9341_WriteCommand(0x2C); // RAMWR
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    LCD_WR_MODE_PWM();
}

uint32_t g_buf_addr;

void lcd_sync(uint32_t scanline)
{
/* --- SYNC PHASE (Consumer Check) --- */
        
        /* A. Wait for Previous Line to Finish */
        /* If the timer is still running (CEN=1), it means it hasn't finished */
        /* sending the previous 256 pixels. We wait here. */
        while (TIM1->CR1 & TIM_CR1_CEN);

        /* B. Check Buffer Safety (Optional but recommended) */
        /* Ensure DMA isn't reading the half we just wrote to. */
        /* (Use your Get_DMA_Buf_Idx logic here if needed) */
        //setx(32, 32 + 255);
        
    if (scanline % 2 == 0)
    {
        HAL_DMA_Abort(htim1.hdma[TIM_DMA_ID_CC2]);
        HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_CC2], 
                    g_buf_addr, 
                    (uint32_t)&GPIOA->ODR, 
                    SCANLINE_SIZE * SCANLINES_PER_BUFFER * 4);
    }
    HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_SET);
    for(volatile int i=0; i<5; i++); // Tiny delay
    HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_RESET);
    LCD_WR_MODE_GPIO();
    ILI9341_SetAddressWindow(32, scanline, 32 + 255, 239);
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    LCD_WR_MODE_PWM();
        /* --- FIRE PHASE --- */
        
        /* Kick the Timer to send exactly 256 pixels */
        /* Because OPM is on, it will run 256 times, then clear CEN automatically. */
    TIM1->CR1 |= TIM_CR1_CEN;
}

IRAM_ATTR void Bus::clock()
{
    frame_latch = !cpu.apu.isBufferHalfFull();
    if (frame_latch == 0) num_displayed_frames++;
    else num_skipped_frames++;

    if (frame_latch == 0) {
        g_buf_addr = (uint32_t)ppu.display_buffer;
        ppu.write_buf_idx = 0;
        ppu.ptr_display = ppu.display_buffer[1];
        HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_RESET);
        set_window();

        HAL_DMA_Abort(htim1.hdma[TIM_DMA_ID_CC2]);
        HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_CC2], 
                    (uint32_t)ppu.display_buffer, 
                    (uint32_t)&GPIOA->ODR, 
                    SCANLINE_SIZE * SCANLINES_PER_BUFFER * 4);
    }

    // 1 frame == 341 dots * 261 scanlines
    // Visible scanlines 0-239
    
    // Rendering 3 scanlines at a time because 1 CPU clock == 3 PPU clocks
    // and there's only 341 ppu clocks (dots) in a scanline, which is not divisible by 3.
    // Using a counter/for loop with += 341 & -= 3 is too big of a performance hit.
    // 1 scanline == ~113.67 CPU clocks, so for every 3 scanlines, two scanlines will have an extra CPU clock
    for (ppu_scanline = 0; ppu_scanline < 240; ppu_scanline += 3)
    {
        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline);
        if (ppu_scanline > 0)
            lcd_sync(ppu_scanline - 1);
        if (ppu_scanline == 0)
        {
            CONFIGURE_BURST_MODE(256*2);
            lcd_sync(ppu_scanline);
        }
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline); 
                if (ppu_scanline > 0)
                    lcd_sync(ppu_scanline - 1);

                if (ppu_scanline == 0)
                {
                    CONFIGURE_BURST_MODE(256*2);
                    lcd_sync(ppu_scanline);
                }
            }
            else { ppu.fakeSpriteHit(ppu_scanline); }
        #endif

        cpu.clock(113);
        //cpu.apu.clock(113/2);

        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline + 1);
        lcd_sync(ppu_scanline);
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline + 1); 
                lcd_sync(ppu_scanline);
            }
            else { ppu.fakeSpriteHit(ppu_scanline + 1); }
        #endif
        cpu.clock(114);
        //cpu.apu.clock(114/2);

        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline + 2);
        lcd_sync(ppu_scanline + 1);
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline + 2); 
                lcd_sync(ppu_scanline + 1);
            }
            else { ppu.fakeSpriteHit(ppu_scanline + 2);  }
        #endif
        cpu.clock(114);
        cpu.apu.clock((113+114+114) / 2);
    }
    while (TIM1->CR1 & TIM_CR1_CEN);
    //cpu.apu.clock(80/2);
    // Setup for the next frame
    // Same reason as scanlines 0-239, 2/3 of scanlines will have an extra CPU clock. 
    // Scanline 240
    //cpu.clock(113);

    // Scanline 241-261
    ppu.setVBlank();
    cpu.clock(2501);

    ppu.clearVBlank();
    //cpu.clock(114);
    //cpu.apu.clock(338/2*240);
    cpu.apu.clock((2501 + 80)/2);

    //if (frame_latch == 0) frame_latch = 1;
    //else frame_latch = frame_latch - 1;
}


IRAM_ATTR void Bus::setPPUMirrorMode(Cartridge::MIRROR mirror)
{
    ppu.setMirror(mirror);
}

Cartridge::MIRROR Bus::getPPUMirrorMode()
{
    return ppu.getMirror();
}

IRAM_ATTR void Bus::OAM_Write(uint8_t addr, uint8_t data)
{
    ppu.ptr_sprite[addr] = data;
}

#if 0
void Bus::connectScreen(TFT_eSPI* screen)
{
    ptr_screen = screen;
}
#endif

bool dma_busy = 0;

static inline void spi_dma_start(void *data)
{
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, /*128*/ 256 * SCANLINES_PER_BUFFER * 2/2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    dma_busy = 1;
}

static inline void spi_dma_wait_ready(void)
{
    if (!dma_busy) return;
    // DMA transfer complete
    while (!LL_DMA_IsActiveFlag_TC2(DMA1)) {}
    LL_DMA_ClearFlag_TC2(DMA1);

    // Must wait until SPI is fully idle
    while (LL_SPI_IsActiveFlag_BSY(SPI1)) {}

    // Required: disable channel so next CNDTR reload takes effect
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    dma_busy = 0;
}

IRAM_ATTR void Bus::renderImage(uint16_t scanline)
{
    /*#ifndef TFT_PARALLEL
        ptr_screen->pushImageDMA(32, scanline, 256, SCANLINES_PER_BUFFER, ppu.ptr_display);
    #else
        ptr_screen->pushImage(32, scanline, 256, SCANLINES_PER_BUFFER, ppu.ptr_display);
    #endif*/
    //if (scanline >= 160) return;
    //ST7735_WriteData((uint8_t*)ppu.ptr_display, 256 * SCANLINES_PER_BUFFER * 2);
    //spi_dma_wait_ready();
    //spi_dma_start(ppu.display_buffer[ppu.write_buf_idx] + ppu.x / 2);
    //spi_dma_init(ppu.display_buffer[ppu.write_buf_idx] + ppu.x, 256 * SCANLINES_PER_BUFFER * 2);
#if 0
    HAL_StatusTypeDef st;
    do {
        st = HAL_SPI_Transmit_DMA(&hspi1, reinterpret_cast<uint8_t*>(ppu.display_buffer[ppu.write_buf_idx] /*ppu.ptr_buffer*/), 128 /*256*/ * SCANLINES_PER_BUFFER * 2);
    } while (st != HAL_OK);
#endif
    ppu.ptr_display = ppu.display_buffer[ppu.write_buf_idx];
    ppu.write_buf_idx ^= 1;
    ppu.ptr_buffer = ppu.display_buffer[ppu.write_buf_idx];

} 


void Bus::insertCartridge(Cartridge* cartridge)
{
    cart = cartridge;
    ppu.connectCartridge(cartridge);
    cart->connectBus(this);
}

void Bus::reset()
{
    //ptr_screen->fillScreen(TFT_BLACK);
	for (auto& i : RAM) i = 0x00;
    cart->reset();
	cpu.reset();
    cpu.apu.reset();
	ppu.reset();
}

IRAM_ATTR void Bus::IRQ()
{
    cpu.IRQ();
}

IRAM_ATTR void Bus::NMI()
{
    cpu.NMI();
}

#if 0
void Bus::saveState()
{
    if (!SD.exists("/states")) SD.mkdir("/states");
    uint32_t CRC32 = cart->CRC32;

    char CRC32_str[9];
    sprintf(CRC32_str, "%08X", CRC32);

    char filename[32];
    sprintf(filename, "/states/%s.state", CRC32_str);

    File state = SD.open(filename, FILE_WRITE);
    if (!state) return;

    // Header for verification - ANEMOIA + CRC32
    state.print("ANEMOIA");
    state.write((const uint8_t*)CRC32_str, 8);

    // Dump state
    state.write(RAM, sizeof(RAM));
    cpu.dumpState(state);
    ppu.dumpState(state);
    cart->dumpState(state);

    state.close();
}

void Bus::loadState()
{
    uint32_t CRC32 = cart->CRC32;

    char CRC32_str[9];
    sprintf(CRC32_str, "%08X", CRC32);

    char filename[32];
    sprintf(filename, "/states/%s.state", CRC32_str);
    if (!SD.exists(filename)) return;

    File state = SD.open(filename, FILE_READ);
    if (!state) return;

    // Verify header
    char header[8];
    char CRC[9];
    state.read((uint8_t*)&header, 7);
    header[7] = '\0';
    state.read((uint8_t*)&CRC, 8);
    CRC[8] = '\0';

    if (strcmp(header, "ANEMOIA") != 0)
    {
        state.close();
        return;
    }
    if (strcmp(CRC, CRC32_str) != 0)
    {
        state.close();
        return;
    }
    
    // Load state
    state.read(RAM, sizeof(RAM));
    cpu.loadState(state);
    ppu.loadState(state);
    cart->loadState(state);

    state.close();
}
#endif
