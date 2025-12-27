#ifndef ANALOG_STICK_HPP
#define ANALOG_STICK_HPP

#include "stm32g4xx_hal.h"
#include <cmath>

struct AnalogStickConfig {
    // --- Analog X Axis Configuration ---
    ADC_HandleTypeDef* hAdcX;      // Handle for X axis ADC (e.g., &hadc1)
    uint32_t channelX;             // ADC Channel for X (e.g., ADC_CHANNEL_5)

    // --- Analog Y Axis Configuration ---
    ADC_HandleTypeDef* hAdcY;      // Handle for Y axis ADC (e.g., &hadc2)
    uint32_t channelY;             // ADC Channel for Y (e.g., ADC_CHANNEL_12)

    // --- Digital Button Configuration ---
    GPIO_TypeDef* btnPort;         // GPIO Port for Button (e.g., GPIOB)
    uint16_t btnPin;               // GPIO Pin for Button (e.g., GPIO_PIN_6)

    // --- Calibration (Optional) ---
    uint32_t deadzone = 100;       // Ignore small movements near center (0-4095 scale)
};

class AnalogStick {
public:
    /**
     * @brief Constructor initializes the driver with hardware config.
     */
    AnalogStick(const AnalogStickConfig& config);

    /**
     * @brief Polls the hardware to update internal state.
     * call this in your main loop or a periodic timer.
     */
    void update();

    /**
     * @brief Get X axis value normalized between -1.0 (Left) and 1.0 (Right).
     */
    float getX() const;

    /**
     * @brief Get Y axis value normalized between -1.0 (Down) and 1.0 (Up).
     */
    float getY() const;

    /**
     * @brief Returns true if the button is currently pressed.
     */
    bool isPressed() const;

private:
    AnalogStickConfig cfg;

    // Raw values (0 - 4095 for 12-bit ADC)
    uint32_t rawX = 2048;
    uint32_t rawY = 2048;
    bool btnState = false;

    // Helper to read a specific ADC channel
    uint32_t readADCChannel(ADC_HandleTypeDef* hadc, uint32_t channel);

    // Helper to normalize raw ADC data to float -1.0 to 1.0
    float normalize(uint32_t rawValue) const;
};

#endif // ANALOG_STICK_HPP

