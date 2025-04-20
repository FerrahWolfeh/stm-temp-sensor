#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_BME280.h> // Required for BME280 sensor object

// --- External Declarations ---
// These variables are defined in sensors.cpp but declared here
// so other files (like display.cpp and main.cpp) can access them.

// Sensor object
extern Adafruit_BME280 bme;

// Sensor value cache (stores the latest readings)
extern float currentTemperature;
extern float currentHumidity;
extern float currentPressureMbar;
extern float currentPressureAtm;
extern float currentAltitude;

// Status flag
extern bool sensorReadError; // True if the last sensor read attempt failed

// LED blink state (controlled by sensor read success)
extern bool ledIsOn;                    // Tracks if the timed blink (after successful read) is active
extern unsigned long ledBlinkStartTime; // When the timed blink started

// --- Function Prototypes ---

/**
 * @brief Initializes the BME280 sensor.
 * @param addr The I2C address of the sensor.
 * @param wire Pointer to the TwoWire interface (e.g., &Wire).
 * @return true if initialization was successful, false otherwise.
 */
bool initializeSensor(uint8_t addr, TwoWire *wire);

/**
 * @brief Reads data from the BME280 sensor and updates the global cache variables.
 *        Also handles the LED indication (ON during read, timed blink on success).
 *        Prints Teleplot formatted data to Serial if successful.
 */
void readSensors();

#endif // SENSORS_H
