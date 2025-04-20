#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware Pin Definitions ---
#define BUTTON_PIN PA0 // User button pin
#define LED_PIN PC13   // Built-in LED pin
#define OLED_RESET -1  // Reset pin for OLED (-1 if not used)
#define I2C_SDA PB7    // I2C Data pin (Default for Blackpill I2C1)
#define I2C_SCL PB6    // I2C Clock pin (Default for Blackpill I2C1)

// --- I2C Addresses ---
#define BME_ADDR 0x76 // BME280 sensor I2C address

// --- Display Configuration ---
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels

// --- Timing Intervals (milliseconds) ---
const unsigned long sensorReadInterval = 1000;   // How often to read the sensor
const unsigned long displayUpdateInterval = 100; // How often to update the display (faster)
const unsigned long ledBlinkDuration = 50;       // How long the LED stays on during blink after successful read
const unsigned long debounceDelay = 50;          // Button debounce time (ms)

#endif // CONFIG_H
