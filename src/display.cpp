#include "display.h"
#include "config.h"  // Include configuration constants (SCREEN_WIDTH, SCREEN_HEIGHT, etc.)
#include "sensors.h" // Include access to sensor data (currentTemperature, etc.) and sensorReadError flag
#include <Arduino.h> // Required for Serial, print functions
#include <Wire.h>    // Required for I2C communication (display)
#include <stdio.h>   // Required for snprintf (string formatting for uptime)

// --- Global Variable Definitions ---
// These are the actual definitions of the variables declared as 'extern' in display.h

// Create the display object instance for SH1106G 128x64 OLED
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Get the singleton instance of the STM32 RTC
STM32RTC &rtc = STM32RTC::getInstance();

// Initialize uptime tracking variable
time_t startTimeEpoch = 0; // Will be set properly in initializeRTC() -> setup()

// Define the total number of display modes
const int NUM_DISPLAY_MODES = 4; // Update this if you add/remove modes in the enum

// Define the initial display mode
DisplayMode currentDisplayMode = PRESSURE_MBAR; // Start by showing pressure in mBar

/**
 * @brief Initializes the SH110X OLED display.
 * @param i2c_addr The I2C address of the display.
 * @param reset True to perform a hardware reset (if RST pin is connected), false otherwise.
 * @return true if initialization was successful, false otherwise.
 */
bool initializeDisplay(uint8_t i2c_addr, bool reset)
{
    // Attempt to initialize the display
    if (!display.begin(i2c_addr, reset))
    {
        // If initialization fails, print an error and return false
        Serial.println(F("SH110X allocation failed"));
        return false;
    }
    // If successful:
    Serial.println("Display Initialized OK.");
    display.display();                     // Show initial buffer (splash screen or blank)
    delay(100);                            // Short delay
    display.clearDisplay();                // Clear the buffer
    display.setTextSize(1);                // Set default text size
    display.setTextColor(SH110X_WHITE);    // Set default text color
    displayCenteredText("Display OK", 28); // Display confirmation message
    display.display();                     // Send buffer to the screen
    delay(500);                            // Pause to show the message
    return true;
}

/**
 * @brief Displays text centered horizontally at a specific vertical position (y).
 * @param text The null-terminated string to display.
 * @param y The vertical coordinate (row) for the top of the text.
 */
void displayCenteredText(const char *text, int y)
{
    int16_t x1, y1; // Variables to store bounding box origin (unused here)
    uint16_t w, h;  // Variables to store text width and height

    // Calculate the bounding box of the text without drawing it
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    // Calculate the x-coordinate to center the text
    int x = (SCREEN_WIDTH - w) / 2;

    // Set the cursor position
    display.setCursor(x, y);

    // Print the text
    display.print(text);
}

/**
 * @brief Clears the display buffer and draws the sensor data or uptime
 *        based on the currentDisplayMode. Uses cached sensor values.
 *        Handles sensor error display. Finally, sends the buffer to the display.
 */
void printSensorData()
{
    // Clear the internal display buffer
    display.clearDisplay();

    // Set text color to white
    display.setTextColor(SH110X_WHITE);
    // Set text size to small (default)
    display.setTextSize(1);

    // Draw a border rectangle around the screen edge (optional visual element)
    // display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);

    // Check if there was a sensor read error AND we are not in UPTIME mode
    if (sensorReadError && currentDisplayMode != UPTIME)
    {
        // If there's an error, display error messages instead of data
        displayCenteredText("BME Sensor Error!", 20);
        displayCenteredText("Check Connection", 35);
    }
    else
    {
        // If no sensor error (or we are in UPTIME mode), display data based on the current mode
        switch (currentDisplayMode)
        {
        case PRESSURE_MBAR:
            // Check if the pressure value is valid (not NaN)
            if (isnan(currentPressureMbar))
            {
                // If invalid, show "Reading..."
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // If valid, display pressure in mBar
                display.setCursor(10, 10); // Set position
                display.print("Press: ");
                display.print(currentPressureMbar, 2); // Value with 2 decimal places
                display.println(" mBar");
            }
            break; // End PRESSURE_MBAR case

        case PRESSURE_ATM:
            // Check if the pressure value is valid
            if (isnan(currentPressureAtm))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // Display pressure in atmospheres
                display.setCursor(10, 10);
                display.print("Press: ");
                display.print(currentPressureAtm, 4); // Value with 4 decimal places
                display.println(" atm");
            }
            break; // End PRESSURE_ATM case

        case ALTITUDE:
            // Check if the altitude value is valid
            if (isnan(currentAltitude))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // Display altitude in meters
                display.setCursor(10, 10);
                display.print("Alt: ");
                display.print(currentAltitude, 1); // Value with 1 decimal place
                display.println(" m");
            }
            break; // End ALTITUDE case

        case UPTIME:
        { // Use braces to create a scope for local variables
            // Get the current time from the RTC as seconds since epoch
            time_t currentEpoch = rtc.getEpoch();
            // Calculate total elapsed seconds since the program started
            // Ensure currentEpoch is not less than startTimeEpoch (can happen briefly during init)
            unsigned long totalSeconds = (currentEpoch >= startTimeEpoch) ? (currentEpoch - startTimeEpoch) : 0;

            // Calculate days, hours, minutes, seconds from total seconds
            int seconds = totalSeconds % 60;
            int minutes = (totalSeconds / 60) % 60;
            int hours = (totalSeconds / 3600) % 24;
            int days = totalSeconds / (3600 * 24);

            // Format the uptime into a string: "D d HH:MM:SS"
            char uptimeStr[25]; // Buffer to hold the formatted string
            snprintf(uptimeStr, sizeof(uptimeStr), "%d d %02d:%02d:%02d", days, hours, minutes, seconds);

            // Display the uptime string centered
            displayCenteredText("Device Uptime", 15);
            displayCenteredText(uptimeStr, 35);
        }
        break; // End UPTIME case
        } // End switch (currentDisplayMode)

        // --- Display Temp & Humidity (Common to non-UPTIME modes) ---
        // If the current mode is NOT UPTIME, also display temperature and humidity
        if (currentDisplayMode != UPTIME)
        {
            // Check if temperature or humidity values are invalid
            if (isnan(currentTemperature) || isnan(currentHumidity))
            {
                // If invalid, display placeholders
                display.setCursor(10, 30);
                display.println("Temp: --- C");
                display.setCursor(10, 50);
                display.println("Hum:  --- %");
            }
            else
            {
                // If valid, display the values
                display.setCursor(10, 30);
                display.print("Temp: ");
                display.print(currentTemperature, 1); // Value with 1 decimal place
                display.println(" C");

                display.setCursor(10, 50);
                display.print("Hum: ");
                display.print(currentHumidity, 1); // Value with 1 decimal place
                display.println(" %");
            }
        } // End if (currentDisplayMode != UPTIME)
    } // End else (no sensor error or UPTIME mode)

    // Send the completed buffer to the physical display
    display.display();
}

/**
 * @brief Initializes the Real-Time Clock (RTC) using the LSE oscillator.
 *        Sets the initial epoch time to 0 and records the start time.
 */
void initializeRTC()
{
    Serial.println("Initializing RTC...");
    // Set the clock source to the Low Speed External oscillator (LSE)
    // This is typically more accurate than LSI for timekeeping.
    // Ensure you have an external 32.768 kHz crystal connected if using LSE.
    rtc.setClockSource(STM32RTC::LSE_CLOCK);

    // Initialize the RTC library
    rtc.begin(); // Use false if you don't want to reset time on boot

    // Short delay to allow RTC to stabilize (optional but sometimes helpful)
    delay(100);

    // Set the RTC's current time to 0 seconds since the epoch (Jan 1, 1970)
    // This effectively resets the uptime counter at the start.
    Serial.println("Setting RTC epoch to 0...");
    rtc.setEpoch(0);

    // Short delay after setting time (optional)
    delay(100);

    // Record the current epoch time immediately after setting it.
    // This 'startTimeEpoch' will be the reference point for calculating uptime.
    startTimeEpoch = rtc.getEpoch();
    Serial.print("RTC setup complete. Start Epoch recorded: ");
    Serial.println(startTimeEpoch); // Should print 0 or a very small number
}
