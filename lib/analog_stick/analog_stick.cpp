#include "analog_stick.h"

AnalogStick::AnalogStick(const AnalogStickConfig& config) : cfg(config) {
    // Initial hardware checks or calibration could go here
}

void AnalogStick::update() {
    // 1. Read Analog X
    rawX = readADCChannel(cfg.hAdcX, cfg.channelX);

    // 2. Read Analog Y
    rawY = readADCChannel(cfg.hAdcY, cfg.channelY);

    // 3. Read Button (Active Low: 0 = Pressed, 1 = Released)
    // We invert the result so true = Pressed
    btnState = (HAL_GPIO_ReadPin(cfg.btnPort, cfg.btnPin) == GPIO_PIN_RESET);
}

float AnalogStick::getX() const {
    return normalize(rawX);
}

float AnalogStick::getY() const {
    return normalize(rawY);
}

bool AnalogStick::isPressed() const {
    return btnState;
}

uint32_t AnalogStick::readADCChannel(ADC_HandleTypeDef* hadc, uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};

    // Configure the channel to be read
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_24CYCLES_5; // Adjust based on your signal impedance
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK) {
        // Handle Error (Optional: Return 2048 as failsafe)
        return 2048;
    }

    // Start Conversion
    HAL_ADC_Start(hadc);

    // Poll for completion (Timeout 10ms)
    uint32_t result = 2048;
    if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
        result = HAL_ADC_GetValue(hadc);
    }

    // Stop ADC
    HAL_ADC_Stop(hadc);

    return result;
}

float AnalogStick::normalize(uint32_t rawValue) const {
    // Assuming 12-bit ADC (0 - 4095)
    // Center is approx 2048
    const float center = 2048.0f;
    const float maxVal = 4095.0f;
    
    // Apply Deadzone
    if (std::abs((int)rawValue - (int)center) < (int)cfg.deadzone) {
        return 0.0f;
    }

    // Normalize to -1.0 ... 1.0
    return ((float)rawValue - center) / center;
}