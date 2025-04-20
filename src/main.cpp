#include <Arduino.h> // Core Arduino framework
#include <Wire.h>    // I2C communication library

// Include our custom header files
#include "config.h"  // Pin definitions, constants, timing intervals
#include "sensors.h" // Sensor functions and variables (readSensors, bme, currentTemperature, etc.)
#include "display.h" // Display functions and variables (printSensorData, display, currentDisplayMode, etc.)

// --- Timing Variables ---
// These track the last time certain actions were performed for non-blocking delays.
unsigned long lastSensorReadTime = 0;    // Last time sensors were read
unsigned long lastDisplayUpdateTime = 0; // Last time the display was updated

// --- Button Debounce Variables ---
// Used to ensure a single button press registers only once.
unsigned long lastDebounceTime = 0; // Time the button pin last changed state
byte buttonState = HIGH;            // Current stable (debounced) state of the button (HIGH = not pressed)
byte lastButtonState = HIGH;        // Previous raw reading from the button pin

/**
 * @brief Setup function: Initializes hardware and software components once on boot.
 */
void setup()
{
    // --- Initialize Hardware Pins ---
    pinMode(LED_PIN, OUTPUT);          // Configure the built-in LED pin as output
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure the button pin as input with internal pull-up resistor
    digitalWrite(LED_PIN, HIGH);       // Turn the LED OFF initially (HIGH = OFF for PC13 on many STM32 boards)

    // --- Initialize Serial Communication ---
    Serial.begin(9600); // Start serial monitor communication at 9600 baud
    while (!Serial && millis() < 5000)
        ; // Wait for Serial port to connect (optional, useful for some boards)
    Serial.println("\n\nStarting Initialization...");
    Serial.println("--------------------------");

    // --- Initialize I2C Communication ---
    // Must be called before initializing I2C devices (sensor, display)
    Wire.begin(); // Join the I2C bus as master

    // --- Initialize Real-Time Clock (RTC) ---
    // Function defined in display.cpp (as it's closely tied to uptime display)
    initializeRTC();
    Serial.println("--------------------------");

    // --- Initialize Display ---
    Serial.println("Initializing Display...");
    // Loop until the display is successfully initialized
    while (!initializeDisplay()) // Function defined in display.cpp
    {
        Serial.println("Retrying Display Init...");
        delay(500); // Wait before retrying
    }
    // Display initialization confirmation is handled within initializeDisplay()
    Serial.println("--------------------------");

    // --- Initialize BME280 Sensor ---
    Serial.println("Initializing BME280 Sensor...");
    // Loop until the sensor is successfully initialized
    while (!initializeSensor(BME_ADDR, &Wire)) // Function defined in sensors.cpp
    {
        Serial.println("Retrying Sensor Init...");
        delay(500); // Wait before retrying
    }
    // Display sensor confirmation message
    display.clearDisplay();
    char bmeOkMsg[25]; // Increased buffer size slightly
    snprintf(bmeOkMsg, sizeof(bmeOkMsg), "BME280 OK (0x%X)", BME_ADDR);
    displayCenteredText(bmeOkMsg, 28);
    display.display();
    delay(1000); // Pause to show the message
    Serial.println("--------------------------");

    // --- Perform Initial Sensor Read ---
    Serial.println("Performing initial sensor read...");
    readSensors(); // Function defined in sensors.cpp
    if (sensorReadError)
    {
        // If the first read fails, print a warning and show error on display
        Serial.println("Initial sensor read failed! LED may remain ON.");
        printSensorData(); // Show error message on display
    }
    else
    {
        // If successful, print confirmation
        Serial.println("Initial sensor read OK.");
    }
    Serial.println("Button handling uses polling.");
    Serial.println("--------------------------");

    // --- Final Setup Steps ---
    display.clearDisplay();
    displayCenteredText("Ready!", 28); // Show "Ready" message
    display.display();
    delay(1000);            // Pause
    display.clearDisplay(); // Clear display before entering loop
    display.display();

    Serial.println("Setup Complete. Entering main loop.");
    Serial.println("==========================");

    // Initialize timing variables for the main loop
    lastSensorReadTime = millis();
    lastDisplayUpdateTime = millis();
    lastButtonState = digitalRead(BUTTON_PIN); // Read initial button state
}

/**
 * @brief Main loop function: Runs repeatedly after setup().
 */
void loop()
{
    // Get the current time at the start of the loop iteration.
    // Using a single timestamp helps keep timing consistent within the loop.
    unsigned long currentMillis = millis();

    // --- Button Debounce Logic (Polling State Machine) ---
    byte reading = digitalRead(BUTTON_PIN); // Read the raw state of the button pin

    // Check if the raw state has changed since the last loop iteration
    if (reading != lastButtonState)
    {
        // If it changed, reset the debounce timer
        lastDebounceTime = currentMillis;
    }

    // Check if enough time has passed since the last state change to consider it stable
    if ((currentMillis - lastDebounceTime) > debounceDelay)
    {
        // If the debounce delay has passed, check if the stable state is different from the current processed state
        if (reading != buttonState)
        {
            buttonState = reading; // Update the stable button state

            // Check if the button was just pressed (transitioned to LOW)
            if (buttonState == LOW)
            {
                Serial.println("Button Pressed (Debounced)"); // Log the debounced press

                // Cycle to the next display mode
                // Cast current mode to int, add 1, use modulo to wrap around, cast back to DisplayMode
                currentDisplayMode = (DisplayMode)(((int)currentDisplayMode + 1) % NUM_DISPLAY_MODES);

                // Immediately update the display to show the new mode
                printSensorData(); // Function defined in display.cpp
                // Reset the display update timer to prevent immediate overwrite by the timed update
                lastDisplayUpdateTime = currentMillis;
            }
            // Optional: Add else block here to detect button release (buttonState == HIGH) if needed.
        }
    }
    // Save the current raw reading for the next loop iteration
    lastButtonState = reading;
    // --- End Button Debounce ---

    // --- Timed Sensor Reading ---
    // Check if the interval since the last sensor read has elapsed
    if (currentMillis - lastSensorReadTime >= sensorReadInterval)
    {
        lastSensorReadTime = currentMillis; // Update the last read time
        readSensors();                      // Call the sensor reading function (defined in sensors.cpp)
                                            // This function also handles Teleplot output and starts the LED blink timer on success.
    }

    // --- Non-blocking LED Blink OFF Logic ---
    // This logic turns the LED OFF after the short 'ledBlinkDuration' following a SUCCESSFUL sensor read.
    // The 'ledIsOn' flag is set to true within readSensors() only on success.
    if (ledIsOn && (currentMillis - ledBlinkStartTime >= ledBlinkDuration))
    {
        // If the timed blink is active (ledIsOn == true) AND enough time has passed:
        // Debug messages for LED timing
        // Serial.print(currentMillis);
        // Serial.print(" : Turning LED OFF. StartTime=");
        // Serial.print(ledBlinkStartTime);
        // Serial.print(", Diff=");
        // Serial.println(currentMillis - ledBlinkStartTime);

        digitalWrite(LED_PIN, HIGH); // Turn the LED OFF
        ledIsOn = false;             // Deactivate the timed blink state (so this check doesn't run again until the next successful read)
    }
    // Note: If sensorReadError is true, ledIsOn remains false, and the LED stays ON
    // because it was turned ON at the beginning of readSensors() and never turned OFF here.

    // --- Timed Display Update ---
    // Check if the interval since the last display update has elapsed
    if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval)
    {
        lastDisplayUpdateTime = currentMillis; // Update the last display time
        printSensorData();                     // Update the display with current data/mode (defined in display.cpp)
    }

} // End of loop()
