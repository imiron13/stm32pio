#pragma once

#include <cstdint>
#include <hal_wrapper_stm32.h>
#include <ll_wrapper_stm32.h>

using namespace std;

extern TIM_HandleTypeDef htim1;

template<class TMR, class GPIO, class DMA>
class DmaGpioPwm_8bitParallelWithWriteStrobe
{
    static inline uint32_t s_line = 0;
    static inline uint32_t s_buf_size = 0;
    static inline uint8_t* s_buf = nullptr;
    static void setupDataDmaCircularMode(uint8_t* buff, size_t buff_size);
    static void setupWriteStrobePulsesPwm(size_t buff_size);

public:
    static void init(uint32_t period = 16);

    static void setupCircularDoubleBufferMode(uint8_t* buff, size_t buff_size);
    static void startTransfer();
    static void doDoubleBufferTransfer();
    static void waitTransferComplete();
};

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::init(uint32_t period)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = period;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        // Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
        // Error_Handler();
    }
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
        // Error_Handler();
    }
    if (HAL_TIM_OnePulse_Init(&htim1, TIM_OPMODE_SINGLE) != HAL_OK)
    {
        // Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        // Error_Handler();
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM2;
    sConfigOC.Pulse = period / 2;
    sConfigOC.OCPolarity = /*TIM_OCPOLARITY_HIGH*/TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = /*TIM_OCIDLESTATE_RESET*/TIM_OCIDLESTATE_SET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        // Error_Handler();
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        // Error_Handler();
    }
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = /*TIM_OSSI_DISABLE*/TIM_OSSI_ENABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter = 0;
    sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
        //Error_Handler();
    }

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA8     ------> TIM1_CH1
    PA9     ------> TIM1_CH2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = /*GPIO_NOPULL*/GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    
    LL_TIM_SetOnePulseMode(TIM1, LL_TIM_ONEPULSEMODE_SINGLE);
    
    // CH1 - WR pulses generation
    LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1);

    // CH2 - DATA DMA trigger
    // DMA is triggered on CH2 compare event: CNT==1
    uint32_t data_dma_trigger_timing = 1;  // number of TIM1 cycles since overflow
    LL_TIM_DisableDMAReq_UPDATE(TIM1);
    LL_TIM_EnableDMAReq_CC2(TIM1);
    LL_TIM_OC_SetCompareCH2(TIM1, data_dma_trigger_timing);

    LL_TIM_EnableAllOutputs(TIM1);
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::setupDataDmaCircularMode(uint8_t* buff, size_t buff_size)
{
    HAL_DMA_Abort(htim1.hdma[TIM_DMA_ID_CC2]);
    HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_CC2], 
                (uint32_t)buff, 
                (uint32_t)&GPIOA->ODR, 
                buff_size);
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::setupWriteStrobePulsesPwm(size_t buff_size)
{
    LL_TIM_SetRepetitionCounter(TIM1, buff_size - 1);

    LL_TIM_GenerateEvent_UPDATE(TIM1);  // to update shadow registers?
    //__HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::setupCircularDoubleBufferMode(uint8_t* buff, size_t buff_size)
{
    s_line = 0;
    s_buf_size = buff_size;
    s_buf = buff;
    setupDataDmaCircularMode(buff, buff_size);
    setupWriteStrobePulsesPwm(buff_size / 2);
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::startTransfer()
{
    LL_TIM_EnableCounter(TIM1);
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::waitTransferComplete()
{
    while (LL_TIM_IsEnabledCounter(TIM1));
}

template<class TMR, class GPIO, class DMA>
void DmaGpioPwm_8bitParallelWithWriteStrobe<TMR, GPIO, DMA>::doDoubleBufferTransfer()
{
    waitTransferComplete();
        
    if (s_line % 2 == 0)
    {
        setupDataDmaCircularMode(s_buf, s_buf_size);  // restart DMA to increase reliability (WR strobe corruption)
    }

    s_line++;
}
