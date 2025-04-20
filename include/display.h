#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_SH110X.h> // Specific driver for SH1106/SH1107
#include <STM32RTC.h>        // Required for RTC functionality (Uptime)

// --- External Declarations ---
// These are defined in display.cpp but declared here for access elsewhere.

// Display object
extern Adafruit_SH1106G display; // Use SH1106G for the specific display type

// RTC instance
extern STM32RTC &rtc; // Reference to the RTC instance

// Uptime tracking
extern time_t startTimeEpoch; // Stores the RTC time at the end of setup()

// --- Display Mode Enum ---
// Defines the different screens the user can cycle through
enum DisplayMode
{
    PRESSURE_MBAR, // Show pressure in millibars (hPa)
    PRESSURE_ATM,  // Show pressure in standard atmospheres
    ALTITUDE,      // Show calculated altitude
    UPTIME         // Show device uptime since boot/reset
};

// Total number of display modes available
extern const int NUM_DISPLAY_MODES;

// Current display mode state variable
extern DisplayMode currentDisplayMode;

// --- Function Prototypes ---

/**
 * @brief Initializes the SH110X OLED display.
 * @param i2c_addr The I2C address of the display.
 * @param reset True to perform a hardware reset (if RST pin is connected), false otherwise.
 * @return true if initialization was successful, false otherwise.
 */
bool initializeDisplay(uint8_t i2c_addr = 0x3C, bool reset = true);

/**
 * @brief Displays text centered horizontally at a specific vertical position (y).
 * @param text The null-terminated string to display.
 * @param y The vertical coordinate (row) for the top of the text.
 */
void displayCenteredText(const char *text, int y);

/**
 * @brief Clears the display buffer and draws the sensor data or uptime
 *        based on the currentDisplayMode. Uses cached sensor values.
 *        Handles sensor error display. Finally, sends the buffer to the display.
 */
void printSensorData();

/**
 * @brief Initializes the Real-Time Clock (RTC) using the LSE oscillator.
 *        Sets the initial epoch time to 0.
 */
void initializeRTC();

#endif // DISPLAY_H
