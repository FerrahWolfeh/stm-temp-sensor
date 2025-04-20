// Purpose: Declares the public interface for the sensor module.
//          It defines the external variables (sensor object, data cache,
//          status flags) and function prototypes that other parts of the
//          program (like main.cpp and display.cpp) can use to interact
//          with the sensor functionality defined in sensors.cpp.
// --------------------------------------------------------------------------
#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_BME280.h> // Required for the Adafruit_BME280 class definition

// --- External Declarations ---
// 'extern' tells the compiler that these variables are *defined* elsewhere
// (in sensors.cpp) but can be accessed from files that include this header.
// This allows sharing data between different source files.

// Sensor object instance
extern Adafruit_BME280 bme; // The actual BME280 sensor object used for communication

// Sensor value cache (stores the latest valid readings)
// Using a cache avoids reading the sensor every time the display updates.
extern float currentTemperature;  // Last measured temperature (°C)
extern float currentHumidity;     // Last measured relative humidity (%)
extern float currentPressureMbar; // Last measured pressure (millibars / hPa)
extern float currentPressureAtm;  // Last measured pressure (standard atmospheres)
extern float currentAltitude;     // Last calculated altitude (meters)

// Status flag
extern bool sensorReadError; // Flag indicating if the *last* sensor read attempt failed (true = error, false = success)

// LED blink state (controlled by sensor read success/failure)
// These manage the non-blocking LED blink indication after a successful read.
extern bool ledIsOn;                    // Tracks if the timed blink (after successful read) is active (true = LED should be ON, waiting for timer to turn OFF)
extern unsigned long ledBlinkStartTime; // Stores the timestamp (from millis()) when the successful read finished, starting the blink timer

// --- Function Prototypes ---
// These declare the functions available in sensors.cpp.

/**
 * @brief Initializes the BME280 sensor communication.
 * @param addr The I2C address of the BME280 sensor (e.g., BME_ADDR from config.h).
 * @param wire Pointer to the TwoWire interface instance (e.g., &Wire) to use for I2C.
 * @return true if initialization was successful, false otherwise.
 */
bool initializeSensor(uint8_t addr, TwoWire *wire);

/**
 * @brief Reads data from the BME280 sensor, updates the global cache variables,
 *        handles the status LED indication, and prints Teleplot formatted data to Serial.
 *        - LED turns ON (active LOW) at the start of the read attempt.
 *        - If read fails, LED stays ON, sensorReadError is set true.
 *        - If read succeeds, cache variables are updated, sensorReadError is set false,
 *          Teleplot data is printed, and a non-blocking LED blink timer is started
 *          (ledIsOn = true, ledBlinkStartTime = current time). The main loop handles turning
 *          the LED OFF after ledBlinkDuration.
 */
void readSensors();

#endif // SENSORS_H