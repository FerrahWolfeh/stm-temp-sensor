// Purpose: Declares the public interface for the display module.
//          It defines the external variables (display object, RTC reference,
//          uptime tracking, display mode state) and function prototypes
//          related to the OLED display and RTC functionality.
// --------------------------------------------------------------------------
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>    // Core graphics library (required by Adafruit display drivers)
#include <Adafruit_SH110X.h> // Specific driver library for SH1106/SH1107 OLED controllers
#include <STM32RTC.h>        // Required for Real-Time Clock functionality (used for uptime)
#include "config.h"          // Core definitions and configs for board

// --- External Declarations ---
// Declaring these variables as 'extern' allows other files (like main.cpp)
// to access the display object, RTC instance, and display state variables
// defined in display.cpp.

// Display object instance
extern Adafruit_SH1106G display; // Specific object type for SH1106G 128x64 OLED

// RTC instance reference
// Using a reference (&) ensures we are working with the single, global RTC instance.
extern STM32RTC &rtc;

// Uptime tracking variable
// Stores the RTC time (seconds since epoch) recorded at the end of the setup() function.
// Used as the baseline for calculating device uptime.
extern time_t startTimeEpoch;

// --- Display Mode Enum ---
// Defines symbolic names for the different information screens the user can cycle through.
enum DisplayMode
{
    PRESSURE_MBAR, // Show pressure in millibars (hPa) + Temp/Humidity
    PRESSURE_ATM,  // Show pressure in standard atmospheres + Temp/Humidity
    ALTITUDE,      // Show calculated altitude + Temp/Humidity
    ALL_PRESS_ALT, // Show Pressure (mBar, atm) and Altitude together on one screen
    UPTIME         // Show device uptime since boot/reset
};

// Total number of display modes available. Must match the number of items in the DisplayMode enum.
extern const int NUM_DISPLAY_MODES; // Defined in display.cpp

// Current display mode state variable
// Tracks which screen is currently selected to be shown. Modified by button presses in main.cpp.
extern DisplayMode currentDisplayMode;

// --- Function Prototypes ---
// These declare the functions available in display.cpp.

/**
 * @brief Initializes the SH110X OLED display.
 * @param i2c_addr The I2C address of the display (default: 0x3C from config.h).
 * @param reset True to perform a hardware reset using the RST pin (if connected), false otherwise (default: true, but uses OLED_RESET pin definition).
 * @return true if initialization was successful, false otherwise.
 */
bool initializeDisplay(uint8_t i2c_addr = SCREEN_ADDR, bool reset = (OLED_RESET != -1));

/**
 * @brief Draws text centered horizontally at a specific vertical position (y) in the display buffer.
 *        IMPORTANT: This function ONLY sets the cursor and prints the text to the internal
 *        display buffer. It does NOT clear the buffer first, nor does it send the buffer
 *        to the physical screen (no display.display() call). Use this when drawing multiple
 *        elements before performing a single screen update.
 * @param text The null-terminated C-string to display.
 * @param y The vertical coordinate (row pixel number) for the top edge of the text.
 */
void displayCenteredText(const char *text, int y);

/**
 * @brief A convenience function to clear the display, draw a single line of centered text,
 *        and immediately update the physical screen.
 *        Handles display.clearDisplay() and display.display() internally.
 *        Do NOT use this function when you need to draw multiple elements before updating the screen,
 *        as it will clear previous drawing operations.
 * @param text The null-terminated C-string to display.
 * @param y The vertical coordinate (row pixel number) for the top edge of the text.
 */
void displayCenteredMessage(const char *text, int y);

/**
 * @brief Clears the display buffer, draws the appropriate sensor data or uptime information
 *        based on the current value of 'currentDisplayMode'. Uses the cached sensor values
 *        (currentTemperature, etc.) from sensors.cpp. Handles displaying an error message
 *        if 'sensorReadError' is true (for most modes). Finally, sends the completed buffer
 *        to the physical display screen.
 */
void printSensorData();

/**
 * @brief Initializes the STM32's built-in Real-Time Clock (RTC).
 *        Configures it to use the Low Speed External (LSE) 32.768 kHz crystal oscillator
 *        for better accuracy (requires an external crystal to be present).
 *        Sets the initial RTC time (epoch) to 0 and records this start time in 'startTimeEpoch'.
 */
void initializeRTC();

#endif // DISPLAY_H
