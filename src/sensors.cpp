// Purpose: Implements the sensor handling logic declared in sensors.h.
//          Contains the definitions for the global sensor variables and
//          the implementation of the sensor initialization and reading functions.
// --------------------------------------------------------------------------
#include "sensors.h" // Include the corresponding header file
#include "config.h"  // Include configuration constants (like BME_ADDR, LED_PIN)
#include <Arduino.h> // Required for Serial, millis(), digitalWrite(), isnan()
#include <Wire.h>    // Required for I2C communication

// --- Global Variable Definitions ---
// These are the actual memory allocations for the variables declared as 'extern' in sensors.h.

Adafruit_BME280 bme; // Create the BME280 sensor object instance.

// Initialize sensor value cache with NAN (Not a Number) to indicate that
// no valid readings have been taken yet. isnan() can be used to check this.
float currentTemperature = NAN;
float currentHumidity = NAN;
float currentPressureMbar = NAN;
float currentPressureAtm = NAN;
float currentAltitude = NAN;

// Initialize status flag
bool sensorReadError = false; // Assume no error initially when the program starts.

// Initialize LED blink state variables for the non-blocking blink after successful read.
bool ledIsOn = false;                // LED timed blink is initially inactive.
unsigned long ledBlinkStartTime = 0; // Initialize blink start time to 0.

/**
 * @brief Initializes the BME280 sensor.
 * @param addr The I2C address of the sensor.
 * @param wire Pointer to the TwoWire interface (e.g., &Wire).
 * @return true if initialization was successful, false otherwise.
 */
bool initializeSensor(uint8_t addr, TwoWire *wire)
{
    // Attempt to begin communication with the BME280 sensor using the specified address and I2C interface.
    if (!bme.begin(addr, wire))
    {
        // If begin() returns false, communication failed.
        Serial.println("BME280 initialization failed! Check wiring or address.");
        return false; // Indicate failure
    }
    // If begin() returns true, communication was established.
    Serial.println("BME280 Sensor Initialized OK.");
    return true; // Indicate success
}

/**
 * @brief Reads data from the BME280 sensor and updates the global cache variables.
 *        Also handles the LED indication (ON during read, timed blink on success).
 *        Prints Teleplot formatted data to Serial if successful.
 */
void readSensors()
{
    // Turn the LED ON (assuming active LOW configuration for PC13) to indicate a read attempt is starting.
    digitalWrite(LED_PIN, LOW);
    // Debug message indicating LED state (optional)
    // Serial.print(millis());
    // Serial.println(" : LED ON (Read Start)");

    // Read sensor values using the Adafruit library functions.
    float temp = bme.readTemperature(); // Read temperature in Celsius.
    float hum = bme.readHumidity();     // Read relative humidity in %.
    float pres = bme.readPressure();    // Read pressure in Pascals (Pa).

    // Check if any reading failed. The library returns NaN (Not a Number) on failure.
    if (isnan(temp) || isnan(hum) || isnan(pres))
    {
        // If any reading failed:
        Serial.println("Sensor read FAILED."); // Print error message to Serial.
        sensorReadError = true;                // Set the global error flag.
        ledIsOn = false;                       // Ensure the timed blink logic in the main loop doesn't activate.
                                               // The LED remains ON (because of the digitalWrite(LOW) above)
                                               // as a persistent indicator of the error state until the next
                                               // successful read attempt clears the flag and starts a blink.
    }
    else
    {
        // If all readings were successful:
        // Debug message indicating success (optional)
        // Serial.println("Sensor read SUCCESS.");

        sensorReadError = false; // Clear the global error flag.

        // Update the global cache variables with the new readings.
        // Apply a naive temperature offset. BME280 self-heating can increase readings by 1-2°C.
        // This simple subtraction is a basic compensation attempt. More advanced compensation might be needed.
        currentTemperature = temp - 2.0F;
        currentHumidity = hum;
        currentPressureMbar = pres / 100.0F;                 // Convert Pascals (Pa) to millibars (mBar), which are equivalent to hectopascals (hPa).
        currentPressureAtm = currentPressureMbar / 1013.25F; // Convert millibars to standard atmospheres (atm) using the standard sea level pressure.
        // Calculate altitude using the library function. It uses the measured pressure and a reference pressure
        // (standard sea level pressure, 1013.25 mbar, by default) to estimate altitude.
        // Note: For more accurate altitude, the current local sea level pressure should be used instead of the default.
        currentAltitude = bme.readAltitude(1013.25);

        // --- Teleplot Formatted Serial Output ---
        // Send data to the Serial port in a format compatible with the Teleplot plotter (https://teleplot.fr/).
        // Format: ">VariableName:Value§Unit\n"
        Serial.print(">Pressure (mBar):");
        Serial.print(currentPressureMbar, 2); // Pressure in mBar with 2 decimal places.
        Serial.println("§mBar");              // Teleplot unit specifier.

        Serial.print(">Pressure (atm):");
        Serial.print(currentPressureAtm, 4); // Pressure in atm with 4 decimal places.
        Serial.println("§atm");              // Teleplot unit specifier.

        Serial.print(">Temperature:");
        Serial.print(currentTemperature, 1); // Temperature in Celsius with 1 decimal place.
        Serial.println("§C");                // Teleplot unit specifier.

        Serial.print(">Humidity:");
        Serial.print(currentHumidity, 1); // Humidity in %RH with 1 decimal place.
        Serial.println("§%");             // Teleplot unit specifier.

        Serial.print(">Altitude:");
        Serial.print(currentAltitude, 1); // Altitude in meters with 1 decimal place.
        Serial.println("§m");             // Teleplot unit specifier.
        // --- End Teleplot Output ---

        // --- Start non-blocking LED blink timer ---
        // Since the read was successful, we want the LED (currently ON) to turn OFF briefly.
        // This sets up a non-blocking timer managed in the main loop.
        ledIsOn = true;               // Activate the timed blink state (tells the main loop to check the timer).
        ledBlinkStartTime = millis(); // Record the time when the successful read finished.
        // Debug message indicating blink timer start (optional)
        // Serial.print(ledBlinkStartTime);
        // Serial.println(" : Blink timer started (LED will turn OFF shortly)");
    }
}