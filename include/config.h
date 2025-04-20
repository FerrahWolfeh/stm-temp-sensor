// Purpose: Defines hardware pin assignments, I2C addresses, display
//          dimensions, and timing constants used throughout the project.
//          Centralizing configuration makes it easier to adapt the code
//          to different hardware setups or timing requirements.
// --------------------------------------------------------------------------
#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware Pin Definitions ---
// These map symbolic names to the actual microcontroller pins used.
#define BUTTON_PIN PA0 // User button pin (connected to GPIO Port A, Pin 0)
#define LED_PIN PC13   // Built-in LED pin (connected to GPIO Port C, Pin 13) - Often active LOW
#define OLED_RESET -1  // Reset pin for OLED display (-1 indicates it's not connected or used)
#define I2C_SDA PB7    // I2C Data pin (connected to GPIO Port B, Pin 7)
#define I2C_SCL PB6    // I2C Clock pin (connected to GPIO Port B, Pin 6)

// --- I2C Addresses ---
// Standard 7-bit I2C addresses for the peripherals.
#define BME_ADDR 0x76    // Default BME280 sensor I2C address (can sometimes be 0x77)
#define SCREEN_ADDR 0x3C // Common SH1106/SSD1306 display I2C address (can sometimes be 0x3D)

// --- Display Configuration ---
// Physical characteristics of the OLED display module.
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels

// --- Timing Intervals (milliseconds) ---
// Defines delays and periods for various non-blocking operations.
const unsigned long sensorReadInterval = 1000;   // How often to read the sensor (1 second)
const unsigned long displayUpdateInterval = 100; // How often to refresh the display (10 times/second) - Faster for responsiveness
const unsigned long ledBlinkDuration = 50;       // How long the LED stays OFF during the blink after a successful read
const unsigned long debounceDelay = 50;          // Button debounce time (ms) - Ignores noisy transitions
const unsigned long buttonHoldDuration = 1000;   // Milliseconds to hold button to trigger the long-press action (sleep mode)

#endif // CONFIG_H