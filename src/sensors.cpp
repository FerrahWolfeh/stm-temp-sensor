#include "sensors.h"
#include "config.h"  // Include configuration constants (like BME_ADDR, LED_PIN)
#include <Arduino.h> // Required for Serial, millis(), digitalWrite(), isnan()
#include <Wire.h>    // Required for I2C communication

// --- Global Variable Definitions ---
// These are the actual definitions of the variables declared as 'extern' in sensors.h

Adafruit_BME280 bme; // Create the BME280 sensor object

// Initialize sensor value cache with NAN (Not a Number)
float currentTemperature = NAN;
float currentHumidity = NAN;
float currentPressureMbar = NAN;
float currentPressureAtm = NAN;
float currentAltitude = NAN;

// Initialize status flag
bool sensorReadError = false; // Assume no error initially

// Initialize LED blink state variables
bool ledIsOn = false;                // LED timed blink is initially inactive
unsigned long ledBlinkStartTime = 0; // Initialize blink start time

/**
 * @brief Initializes the BME280 sensor.
 * @param addr The I2C address of the sensor.
 * @param wire Pointer to the TwoWire interface (e.g., &Wire).
 * @return true if initialization was successful, false otherwise.
 */
bool initializeSensor(uint8_t addr, TwoWire *wire)
{
    // Attempt to begin communication with the BME280 sensor
    if (!bme.begin(addr, wire))
    {
        // If begin() fails, print an error message and return false
        Serial.println("BME280 initialization failed! Check wiring or address.");
        return false;
    }
    // If successful, print a confirmation message and return true
    Serial.println("BME280 Sensor Initialized OK.");
    return true;
}

/**
 * @brief Reads data from the BME280 sensor and updates the global cache variables.
 *        Also handles the LED indication (ON during read, timed blink on success).
 *        Prints Teleplot formatted data to Serial if successful.
 */
void readSensors()
{
    // Turn the LED ON (active LOW) to indicate a read attempt is starting
    digitalWrite(LED_PIN, LOW);
    // Debug message indicating LED state
    // Serial.print(millis());
    // Serial.println(" : LED ON (Read Start)");

    // Read sensor values
    float temp = bme.readTemperature(); // Read temperature
    float hum = bme.readHumidity();     // Read humidity
    float pres = bme.readPressure();    // Read pressure in Pascals

    // Check if any reading failed (returned NaN)
    if (isnan(temp) || isnan(hum) || isnan(pres))
    {
        // If any reading failed:
        Serial.println("Sensor read FAILED."); // Print error message
        sensorReadError = true;                // Set the error flag
        ledIsOn = false;                       // Ensure the timed blink logic doesn't interfere (LED stays solid ON due to the digitalWrite above)
        // Note: The LED will remain ON until the *next* successful read or manual intervention,
        // serving as a persistent error indicator. Consider adding logic elsewhere to turn it off
        // if the error persists for too long or if a different indication is desired.
    }
    else
    {
        // If all readings were successful:
        // Debug message indicating success
        // Serial.println("Sensor read SUCCESS.");

        sensorReadError = false; // Clear the error flag

        // Update the global cache variables with the new readings
        currentTemperature = temp;
        currentHumidity = hum;
        currentPressureMbar = pres / 100.0F;                 // Convert Pascals to millibars (hPa)
        currentPressureAtm = currentPressureMbar / 1013.25F; // Convert millibars to standard atmospheres
        currentAltitude = bme.readAltitude(1013.25);         // Calculate altitude based on standard pressure (1013.25 mbar)
                                                             // Note: For more accurate altitude, use local sea level pressure

        // --- Teleplot Formatted Serial Output ---
        // Send data to Serial in a format compatible with Teleplot (https://teleplot.fr/)
        Serial.print(">Pressure (mBar):");
        Serial.print(currentPressureMbar, 2); // Pressure in mBar with 2 decimal places
        Serial.println("§mBar");              // Teleplot unit specifier

        Serial.print(">Pressure (atm):");
        Serial.print(currentPressureAtm, 4); // Pressure in atm with 4 decimal places
        Serial.println("§atm");              // Teleplot unit specifier

        Serial.print(">Temperature:");
        Serial.print(currentTemperature, 1); // Temperature in Celsius with 1 decimal place
        Serial.println("§C");                // Teleplot unit specifier

        Serial.print(">Humidity:");
        Serial.print(currentHumidity, 1); // Humidity in %RH with 1 decimal place
        Serial.println("§%");             // Teleplot unit specifier

        Serial.print(">Altitude:");
        Serial.print(currentAltitude, 1); // Altitude in meters with 1 decimal place
        Serial.println("§m");             // Teleplot unit specifier
        // --- End Teleplot Output ---

        // --- Start non-blocking LED blink timer ---
        // Since the read was successful, we want the LED to blink briefly OFF
        // The LED is currently ON (from the start of this function).
        // We record the current time and set a flag. The main loop will check this
        // flag and time to turn the LED OFF after ledBlinkDuration.
        ledIsOn = true;               // Activate the timed blink state
        ledBlinkStartTime = millis(); // Record the time when the successful read finished
        // Debug message indicating blink timer start
        // Serial.print(ledBlinkStartTime);
        // Serial.println(" : Blink timer started (LED will turn OFF shortly)");
    }
}
