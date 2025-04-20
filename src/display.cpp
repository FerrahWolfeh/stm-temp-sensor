// Purpose: Implements the display and RTC handling logic declared in display.h.
//          Contains the definitions for the global display/RTC variables and
//          the implementation of the display initialization, drawing, mode handling,
//          and RTC initialization functions.
// --------------------------------------------------------------------------
#include "display.h" // Include the corresponding header file
#include "config.h"  // Include configuration constants (SCREEN_WIDTH, SCREEN_HEIGHT, etc.)
#include "sensors.h" // Include access to sensor data (currentTemperature, etc.) and sensorReadError flag
#include <Arduino.h> // Required for Serial, print functions, isnan
#include <Wire.h>    // Required for I2C communication (display)
#include <stdio.h>   // Required for snprintf (used for formatting the uptime string)

// --- Global Variable Definitions ---
// These are the actual memory allocations for the variables declared as 'extern' in display.h.

// Create the display object instance for the specific SH1106G 128x64 OLED, using the Wire I2C interface.
// OLED_RESET is passed from config.h (-1 means reset pin is not used).
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Get the singleton instance of the STM32 RTC library.
// The library ensures only one instance exists. Using a reference avoids copying.
STM32RTC &rtc = STM32RTC::getInstance();

// Initialize uptime tracking variable. It will be set properly in initializeRTC(), which is called during setup().
time_t startTimeEpoch = 0;

// Define the total number of display modes. MUST match the enum in display.h.
const int NUM_DISPLAY_MODES = 5; // PRESSURE_MBAR, PRESSURE_ATM, ALTITUDE, ALL_PRESS_ALT, UPTIME

// Define the initial display mode when the device boots up.
DisplayMode currentDisplayMode = PRESSURE_MBAR; // Start by showing pressure in mBar

// --- Forward Declarations ---
// Declare displayCenteredText here because displayCenteredMessage uses it, and
// displayCenteredMessage is defined before displayCenteredText in this file.
// (Alternatively, reorder the function definitions).
void displayCenteredText(const char *text, int y);

/**
 * @brief Initializes the SH110X OLED display.
 * @param i2c_addr The I2C address of the display.
 * @param reset True to perform a hardware reset (if RST pin is connected), false otherwise.
 * @return true if initialization was successful, false otherwise.
 */
bool initializeDisplay(uint8_t i2c_addr, bool reset)
{
    // Attempt to initialize the display using the provided I2C address and reset option.
    // The library handles the I2C communication sequence.
    if (!display.begin(i2c_addr, reset))
    {
        // If begin() returns false, initialization failed (e.g., display not found at the address).
        Serial.println(F("SH110X allocation failed")); // F() macro saves RAM by storing string in Flash
        return false;                                  // Indicate failure
    }
    // If successful:
    Serial.println("Display Initialized OK.");
    display.display();                  // Show the initial buffer contents (Adafruit splash screen or blank) on the physical display.
    delay(100);                         // Short delay to allow the display to stabilize or show the splash screen.
    display.clearDisplay();             // Clear the internal display buffer (all pixels off).
    display.setTextSize(1);             // Set the default text size (1 = 6x8 pixels font).
    display.setTextColor(SH110X_WHITE); // Set the default text color (WHITE pixels on BLACK background).
    display.display();                  // Update the physical display to show the cleared screen.

    // Use the convenience function to display a confirmation message centered on the screen.
    // This function handles clear, draw, and display internally.
    displayCenteredMessage("Display OK", 28); // Display "Display OK" near the vertical center.

    delay(500);  // Pause briefly to allow the user to see the confirmation message.
    return true; // Indicate success
}

/**
 * @brief Displays text centered horizontally at a specific vertical position (y).
 *        This function ONLY sets the cursor and prints the text to the buffer.
 *        It does NOT clear the display or send the buffer to the screen.
 *        Use this function when drawing multiple elements before a final display() call.
 * @param text The null-terminated string to display.
 * @param y The vertical coordinate (row) for the top of the text.
 */
void displayCenteredText(const char *text, int y)
{
    int16_t x1, y1; // Variables to store the top-left corner of the bounding box (not used here).
    uint16_t w, h;  // Variables to store the calculated width and height of the text.

    // Calculate the dimensions (width 'w' and height 'h') the text would occupy if drawn at (0,0),
    // without actually drawing it yet.
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    // Calculate the horizontal starting position (x-coordinate) to center the text.
    // x = (Screen Width - Text Width) / 2
    // Ensure x is not negative if the text happens to be wider than the screen (unlikely with centering).
    int x = (SCREEN_WIDTH > w) ? (SCREEN_WIDTH - w) / 2 : 0;

    // Set the cursor position in the display buffer where the text drawing will begin.
    display.setCursor(x, y);

    // Print the text to the display buffer at the calculated cursor position.
    display.print(text);
    // Note: display.display() must be called later to show this on the screen.
}

/**
 * @brief Clears the display, draws centered text, and updates the screen.
 *        This is a convenience function for displaying a single, simple centered message.
 *        It handles the clearDisplay() and display() calls internally.
 *        Do NOT use this function when you need to draw multiple elements before updating the screen.
 * @param text The null-terminated string to display.
 * @param y The vertical coordinate (row) for the top of the text.
 */
void displayCenteredMessage(const char *text, int y)
{
    display.clearDisplay(); // Clear the entire display buffer first.
    // Note: Text size and color should be set beforehand if defaults (size 1, white) are not desired.
    displayCenteredText(text, y); // Use the other function to calculate position and draw the text to the buffer.
    display.display();            // Send the contents of the buffer (now containing the centered text) to the physical display.
}

/**
 * @brief Clears the display buffer and draws the sensor data or uptime
 *        based on the currentDisplayMode. Uses cached sensor values.
 *        Handles sensor error display. Finally, sends the buffer to the display.
 *        Uses the original displayCenteredText for drawing parts of the screen.
 */
void printSensorData()
{
    // 1. Clear the internal display buffer before drawing new content.
    display.clearDisplay();

    // 2. Set default text properties for this screen update.
    display.setTextColor(SH110X_WHITE); // Set text color to white.
    display.setTextSize(1);             // Set text size to small (6x8 font).

    // Optional: Draw a border rectangle around the screen edge for visual separation.
    // display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);

    // 3. Check for Sensor Error (but allow UPTIME mode to display regardless)
    // If there was a sensor read error AND the current mode is NOT UPTIME,
    // display an error message instead of potentially invalid sensor data.
    if (sensorReadError && currentDisplayMode != UPTIME)
    {
        // Display a two-line error message centered on the screen.
        // Use displayCenteredText because we draw multiple lines before the final display() call.
        displayCenteredText("BME Sensor Error!", 20);
        displayCenteredText("Check Connection", 35);
    }
    else
    {
        // 4. No Sensor Error OR Uptime Mode: Display data based on the current mode.
        switch (currentDisplayMode)
        {
        case PRESSURE_MBAR:
            // Check if the cached pressure value is valid (not NaN). NaN indicates the first read hasn't happened yet.
            if (isnan(currentPressureMbar))
            {
                // If invalid (e.g., initial state), show "Reading..." centered.
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // If valid, display pressure in mBar.
                display.setCursor(10, 10); // Set position (indent 10px, row 10).
                display.print("Press: ");
                display.print(currentPressureMbar, 2); // Print value with 2 decimal places.
                display.println(" mBar");              // Add unit and move to next line (though not strictly needed here).
                // Temp & Humidity will be added later in the common section.
            }
            break; // End PRESSURE_MBAR case

        case PRESSURE_ATM:
            // Check if the cached pressure value is valid.
            if (isnan(currentPressureAtm))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // Display pressure in standard atmospheres.
                display.setCursor(10, 10);
                display.print("Press: ");
                display.print(currentPressureAtm, 4); // Print value with 4 decimal places for more precision.
                display.println(" atm");
                // Temp & Humidity will be added later.
            }
            break; // End PRESSURE_ATM case

        case ALTITUDE:
            // Check if the cached altitude value is valid.
            if (isnan(currentAltitude))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                // Display altitude in meters.
                display.setCursor(10, 10);
                display.print("Alt: ");
                display.print(currentAltitude, 1); // Print value with 1 decimal place.
                display.println(" m");
                // Temp & Humidity will be added later.
            }
            break; // End ALTITUDE case

        // --- NEW DISPLAY MODE ---
        case ALL_PRESS_ALT:
            // Display Pressure (mBar), Pressure (atm), and Altitude together on three lines.
            // Check for sensor error specifically for this mode if needed,
            // otherwise display placeholders "---" if data is NaN.
            if (sensorReadError) // Check error flag again, as this mode relies heavily on sensor data
            {
                // Optionally show a specific error message for this combined screen
                displayCenteredText("Sensor Error", 28);
            }
            else
            {
                // Line 1: Pressure (mBar)
                display.setCursor(5, 10); // Position for the first line (indent 5px, row 10)
                display.print("Pr(mBar): ");
                if (isnan(currentPressureMbar))
                {
                    display.print("---"); // Placeholder if NaN
                }
                else
                {
                    display.print(currentPressureMbar, 2);
                }

                // Line 2: Pressure (atm)
                display.setCursor(5, 25); // Position for the second line (row 25)
                display.print("Pr(atm): ");
                if (isnan(currentPressureAtm))
                {
                    display.print("---"); // Placeholder if NaN
                }
                else
                {
                    display.print(currentPressureAtm, 4);
                }

                // Line 3: Altitude (m)
                display.setCursor(5, 40);   // Position for the third line (row 40)
                display.print("Alt(m):  "); // Add extra space for visual alignment with labels above
                if (isnan(currentAltitude))
                {
                    display.print("---"); // Placeholder if NaN
                }
                else
                {
                    display.print(currentAltitude, 1);
                }
            }
            break; // End ALL_PRESS_ALT case
            // --- END NEW DISPLAY MODE ---

        case UPTIME:
        { // Use curly braces to create a local scope for variables inside this case.
            // Get the current time from the RTC as seconds since the epoch (Jan 1, 1970).
            time_t currentEpoch = rtc.getEpoch();
            // Calculate total elapsed seconds since the program started (startTimeEpoch was recorded in setup).
            // Ensure currentEpoch is not less than startTimeEpoch (can happen briefly during initialization if clocks aren't perfectly synced).
            unsigned long totalSeconds = (currentEpoch >= startTimeEpoch) ? (currentEpoch - startTimeEpoch) : 0;

            // Calculate days, hours, minutes, seconds from the total elapsed seconds.
            int seconds = totalSeconds % 60;
            int minutes = (totalSeconds / 60) % 60;
            int hours = (totalSeconds / 3600) % 24;
            int days = totalSeconds / (3600 * 24);

            // Format the uptime into a string using snprintf (safe version of sprintf).
            // Format: "D d HH:MM:SS" (e.g., "3 d 05:12:30")
            char uptimeStr[25]; // Buffer to hold the formatted string (size should be sufficient).
            // %d for days, %02d for hours/minutes/seconds to ensure leading zeros.
            snprintf(uptimeStr, sizeof(uptimeStr), "%d d %02d:%02d:%02d", days, hours, minutes, seconds);

            // Display the uptime string centered on the screen using displayCenteredText.
            // Draw two lines before the final display() call at the end of printSensorData().
            displayCenteredText("Device Uptime", 15); // Title line
            displayCenteredText(uptimeStr, 35);       // Formatted uptime string
        }
        break; // End UPTIME case
        } // End switch (currentDisplayMode)

        // --- Display Temp & Humidity (Common to most modes) ---
        // If the current mode is NOT UPTIME and NOT ALL_PRESS_ALT, also display
        // the temperature and humidity readings at the bottom of the screen.
        if (currentDisplayMode != UPTIME && currentDisplayMode != ALL_PRESS_ALT)
        {
            // Check if temperature or humidity values are invalid (NaN).
            if (isnan(currentTemperature) || isnan(currentHumidity))
            {
                // If invalid (e.g., initial state), display placeholders "---".
                display.setCursor(10, 30); // Position for temperature line
                display.println("Temp: --- C");
                display.setCursor(10, 50); // Position for humidity line
                display.println("Hum:  --- %");
            }
            else
            {
                // If valid, display the actual cached values.
                display.setCursor(10, 30); // Position for temperature line
                display.print("Temp: ");
                display.print(currentTemperature, 1); // Value with 1 decimal place.
                display.println(" C");

                display.setCursor(10, 50); // Position for humidity line
                display.print("Hum: ");
                display.print(currentHumidity, 1); // Value with 1 decimal place.
                display.println(" %");
            }
        } // End if (currentDisplayMode != UPTIME && currentDisplayMode != ALL_PRESS_ALT)
    } // End else (no sensor error or UPTIME/ALL_PRESS_ALT mode)

    // 5. Send the completed buffer to the physical display.
    // This single call updates the screen with everything drawn in the buffer during this function execution.
    display.display();
}

/**
 * @brief Initializes the Real-Time Clock (RTC) using the LSE oscillator.
 *        Sets the initial epoch time to 0 and records the start time.
 */
void initializeRTC()
{
    Serial.println("Initializing RTC...");
    // Set the clock source for the RTC to the Low Speed External oscillator (LSE).
    // This typically uses a 32.768 kHz crystal connected to the microcontroller's
    // LSE pins and provides better accuracy for timekeeping than the internal LSI oscillator.
    // Ensure the crystal is physically present and correctly connected on the board.
    rtc.setClockSource(STM32RTC::LSE_CLOCK);

    // Initialize the RTC library.
    // `false` argument might prevent resetting time if it was already set (e.g., by battery backup),
    // but here we want to reset it for uptime calculation. Default is `true`.
    rtc.begin();

    // Short delay to allow the RTC oscillator to stabilize (optional but can be helpful).
    delay(100);

    // Set the RTC's current time to 0 seconds since the Unix epoch (January 1, 1970, 00:00:00 UTC).
    // This effectively resets the time counter at the start of the program, making it suitable
    // for measuring uptime from this point forward.
    Serial.println("Setting RTC epoch to 0...");
    rtc.setEpoch(0);

    // Short delay after setting the time (optional).
    delay(100);

    // Record the current epoch time immediately after setting it to 0.
    // This 'startTimeEpoch' value (which should be 0 or very close to it) will serve as the
    // reference point (t=0) for calculating the device's uptime later in printSensorData().
    startTimeEpoch = rtc.getEpoch();
    Serial.print("RTC setup complete. Start Epoch recorded: ");
    Serial.println(startTimeEpoch); // Should print 0 or a very small number like 1 if there was a slight delay.
}