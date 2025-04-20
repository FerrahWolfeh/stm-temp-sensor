// Purpose: The main entry point and control center of the application.
//          It initializes all hardware and software components in setup()
//          and then runs the main loop() which handles timing, button input,
//          low-power modes, and calls sensor reading and display update functions
//          at appropriate intervals.
// --------------------------------------------------------------------------
#include <Arduino.h>       // Core Arduino framework (pinMode, digitalRead, digitalWrite, millis, delay, Serial, etc.)
#include <Wire.h>          // I2C communication library (used by sensor and display)
#include <STM32LowPower.h> // Library for accessing STM32 low-power modes (sleep, deep sleep, shutdown)

// Include our custom header files to access their functions and variables
#include "config.h"      // Project-specific configurations (pins, addresses, timings)
#include "sensors.h"     // Sensor functions (initializeSensor, readSensors) and variables (sensorReadError, ledIsOn, etc.)
#include "display.h"     // Display functions (initializeDisplay, printSensorData, etc.) and variables (currentDisplayMode, etc.)
#include "i2c_scanner.h" // Utility function to scan and list devices on the I2C bus (code not provided, assumed to exist)

// --- Timing Variables ---
// Used for non-blocking delays (millis()-based timing). Stores the last time an action was performed.
unsigned long lastSensorReadTime = 0;    // Timestamp of the last sensor read attempt.
unsigned long lastDisplayUpdateTime = 0; // Timestamp of the last display refresh.

// --- Button Debounce and Hold Variables ---
// Manage reading the button state reliably, ignoring noise, and detecting short vs. long presses.
unsigned long lastDebounceTime = 0;     // Timestamp when the button pin last changed state (raw reading).
byte buttonState = HIGH;                // Current stable (debounced) state of the button (HIGH = not pressed, LOW = pressed, due to INPUT_PULLUP).
byte lastButtonState = HIGH;            // Previous raw reading from the button pin (used to detect changes).
unsigned long buttonPressStartTime = 0; // Timestamp when the button was initially pressed (LOW state detected after debounce). Used for hold duration calculation.
bool buttonHeldTriggered = false;       // Flag to ensure the long-press (hold) action triggers only once per physical hold event. Reset on release.

// --- Power State Variable ---
// Tracks whether the system is currently in or transitioning into low-power mode.
bool isLowPowerMode = false; // Flag: true = entering/in sleep mode, false = active mode.

// --- Wakeup Callback Function ---
// This function is registered as the interrupt service routine (ISR) for the wakeup pin.
// It MUST be short, fast, and avoid complex operations (like Serial prints or delays).
// In this case, waking up from LowPower.sleep() automatically resumes execution after the sleep() call,
// so this callback doesn't need to do anything specific. It just needs to exist.
void wakeUpCallback()
{
    // Intentionally empty. Execution resumes after LowPower.sleep() in loop().
}

/**
 * @brief Setup function: Initializes hardware and software components once on boot/reset.
 */
void setup()
{
    // --- 1. Initialize Hardware Pins ---
    pinMode(LED_PIN, OUTPUT);          // Configure the built-in LED pin (PC13) as an output.
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure the button pin (PA0) as input with the internal pull-up resistor enabled.
                                       // This means the pin reads HIGH when the button is NOT pressed, and LOW when pressed (connected to GND).
    digitalWrite(LED_PIN, HIGH);       // Turn the LED OFF initially (PC13 on many STM32 boards is active LOW, so HIGH turns it OFF).

    // --- 2. Initialize Serial Communication ---
    Serial.begin(9600); // Start serial monitor communication at 9600 baud rate.
    // Optional: Wait for Serial port to connect, with a timeout. Useful for boards where Serial needs time.
    // while (!Serial && millis() < 5000);
    Serial.println("\n\nStarting Initialization...");
    Serial.println("--------------------------");

    // --- 3. Initialize Low Power Library ---
    Serial.println("Initializing Low Power...");
    LowPower.begin(); // Initialize the low-power library.

    // Configure a wakeup source: Attach an interrupt to the button pin.
    // This allows the button press (or release in this case) to wake the MCU from sleep mode.
    // - BUTTON_PIN: The pin to monitor (PA0).
    // - wakeUpCallback: The function to call when the interrupt occurs (ISR).
    // - RISING: Trigger the interrupt when the pin goes from LOW to HIGH (button release, because of INPUT_PULLUP).
    // - SLEEP_MODE: Specify the low-power mode this interrupt is intended to wake from. Must match the mode used in LowPower.sleep() or LowPower.deepSleep().
    //   Note: SLEEP_MODE is a relatively light sleep state. DEEPSLEEP_MODE saves more power but might reset more peripherals.
    LowPower.attachInterruptWakeup(BUTTON_PIN, wakeUpCallback, RISING, SLEEP_MODE);
    Serial.println("Low Power Initialized. Button configured for wakeup on release (RISING edge).");
    Serial.println("--------------------------");

    // --- 4. Initialize I2C Communication ---
    // IMPORTANT: Set the I2C pins *before* calling Wire.begin() on STM32.
    Serial.print("Configuring I2C pins (SDA=");
    Serial.print(digitalPinToPinName(I2C_SDA)); // Print the pin name (e.g., PB7) for clarity.
    Serial.print(", SCL=");
    Serial.print(digitalPinToPinName(I2C_SCL)); // e.g., PB6
    Serial.println(")...");
    Wire.setSDA(I2C_SDA); // Set the I2C Data pin explicitly.
    Wire.setSCL(I2C_SCL); // Set the I2C Clock pin explicitly.
    Wire.begin();         // Initialize the I2C library and join the bus as a master device.
    Serial.print("I2C Initialized on ");
    Serial.print(digitalPinToPinName(I2C_SDA));
    Serial.print("/");
    Serial.print(digitalPinToPinName(I2C_SCL));
    Serial.println(".");
    Serial.println("--------------------------");

    // --- 5. Initialize Real-Time Clock (RTC) ---
    // Calls the function defined in display.cpp to set up the RTC with LSE and reset the epoch.
    initializeRTC();
    Serial.println("--------------------------");

    // --- 6. Scan I2C Bus (Optional Debug Step) ---
    // Calls the function from i2c_scanner.cpp (code not shown) to detect and print addresses of connected I2C devices.
    // Useful for verifying sensor/display connectivity and addresses.
    scanI2Cbus();
    Serial.println("--------------------------");

    // --- 7. Initialize Display ---
    Serial.println("Initializing Display...");
    // Attempt to initialize the display using the address from config.h.
    // Retry loop: Keep trying until the display initializes successfully.
    // This prevents the program from halting if the display isn't immediately ready.
    // Pass the reset condition based on whether OLED_RESET is defined (-1 means false).
    while (!initializeDisplay(SCREEN_ADDR, (OLED_RESET != -1)))
    {
        Serial.println("Retrying Display Init...");
        delay(500); // Wait before retrying.
    }
    // Display initialization confirmation message was shown inside initializeDisplay().
    Serial.println("--------------------------");

    // --- 8. Initialize BME280 Sensor ---
    Serial.println("Initializing BME280 Sensor...");
    // Attempt to initialize the sensor using its address and the Wire I2C interface.
    // Retry loop: Keep trying until the sensor initializes successfully.
    while (!initializeSensor(BME_ADDR, &Wire))
    {
        Serial.println("Retrying Sensor Init...");
        delay(500); // Wait before retrying.
    }
    // Display a confirmation message on the OLED screen after successful sensor init.
    display.clearDisplay(); // Clear previous message ("Display OK")
    char bmeOkMsg[25];      // Buffer for formatted string
    // Create a message like "BME280 OK (0x76)"
    snprintf(bmeOkMsg, sizeof(bmeOkMsg), "BME280 OK (0x%X)", BME_ADDR);
    displayCenteredText(bmeOkMsg, 28); // Draw the message to the buffer
    display.display();                 // Show the message on the screen
    delay(1000);                       // Pause to let user see the message
    Serial.println("--------------------------");

    // --- 9. Perform Initial Sensor Read ---
    Serial.println("Performing initial sensor read...");
    readSensors(); // Call the sensor read function to populate the cache variables.
    if (sensorReadError)
    {
        // If the first read failed, report it and show the error on the display.
        Serial.println("Initial sensor read failed! LED may remain ON.");
        printSensorData(); // Update display (will show the error message).
    }
    else
    {
        // If successful, report it. The LED will do its short blink.
        Serial.println("Initial sensor read OK.");
    }
    Serial.println("Button handling uses polling with debounce and hold detection.");
    Serial.println("Hold button for > " + String(buttonHoldDuration) + "ms to enter/exit SLEEP mode."); // Clarify hold action
    Serial.println("--------------------------");

    // --- 10. Final Setup Steps ---
    // Display a brief "Ready!" message.
    display.clearDisplay();
    displayCenteredText("Ready!", 28);
    display.display();
    delay(1000);
    // Clear the display ready for the first data screen in the loop.
    display.clearDisplay();
    display.display();

    Serial.println("Setup Complete. Entering main loop.");
    Serial.println("==========================");

    // --- 11. Initialize Loop Timing Variables ---
    // Set the initial 'last time' for timed actions to the current time to ensure
    // the correct interval passes before the first execution in the loop.
    unsigned long now = millis();
    lastSensorReadTime = now;
    lastDisplayUpdateTime = now;
    // Read the initial button state to prevent false triggers at the start.
    lastButtonState = digitalRead(BUTTON_PIN);
}

/**
 * @brief Main loop function: Runs repeatedly after setup(). Handles all runtime tasks.
 */
void loop()
{
    // Get the current time at the start of this loop iteration. Used for timing checks.
    unsigned long currentMillis = millis();

    // --- Button Debounce and Hold Logic ---
    // 1. Read the raw, instantaneous state of the button pin.
    byte reading = digitalRead(BUTTON_PIN); // HIGH = not pressed, LOW = pressed

    // 2. Check if the raw state has changed since the last loop iteration.
    if (reading != lastButtonState)
    {
        // If it changed, reset the debounce timer. This restarts the waiting period
        // to ensure the state is stable.
        lastDebounceTime = currentMillis;
    }

    // 3. Check if enough time has passed since the last raw state change (debounce delay).
    if ((currentMillis - lastDebounceTime) > debounceDelay)
    {
        // If the debounce delay has passed, we consider the current 'reading' to be stable.
        // Now, check if this stable state is different from the previously known stable state ('buttonState').
        if (reading != buttonState)
        {
            // The debounced state has changed! Update the stable state.
            buttonState = reading;

            // --- Button Press Detected (Falling Edge: HIGH -> LOW) ---
            if (buttonState == LOW)
            {
                Serial.println("Button Pressed (Debounced)");
                // Record the time when the button was confirmed as pressed.
                buttonPressStartTime = currentMillis;
                // Reset the flag that tracks if the 'hold' action has been triggered for this press.
                buttonHeldTriggered = false;
            }
            // --- Button Release Detected (Rising Edge: LOW -> HIGH) ---
            else // buttonState == HIGH
            {
                Serial.println("Button Released (Debounced)");
                // Check if this release corresponds to a SHORT press:
                // Condition 1: The 'hold' action was *not* triggered during this press (buttonHeldTriggered is false).
                // Condition 2: The time elapsed since the press started is less than the defined hold duration.
                if (!buttonHeldTriggered && (currentMillis - buttonPressStartTime < buttonHoldDuration))
                {
                    // --- Short Press Action ---
                    Serial.println("Short Press Detected.");
                    // Action: Cycle through the display modes.
                    // This action now occurs on release, regardless of whether we just woke from sleep or not.
                    Serial.println("Cycling display mode...");
                    // Get the integer value of the current mode, add 1, wrap around using modulo NUM_DISPLAY_MODES,
                    // and cast back to the DisplayMode enum type.
                    currentDisplayMode = (DisplayMode)(((int)currentDisplayMode + 1) % NUM_DISPLAY_MODES);

                    // Immediately update the display to show the new mode's content.
                    printSensorData(); // Call the display update function.

                    // Reset the display update timer to prevent the regular timed update
                    // from potentially overwriting this change immediately.
                    lastDisplayUpdateTime = currentMillis;
                }
                // Reset the press start time after the button is released, regardless of whether it was short or long.
                // This prepares for the next press event.
                buttonPressStartTime = 0;
                // The buttonHeldTriggered flag is implicitly reset for the next press when buttonState goes LOW again.
            }
        } // End of debounced state change handling
    } // End of debounce check

    // --- Hold Detection (While Button is Held Down) ---
    // This check runs continuously while the debounced button state is LOW.
    // Check if:
    // 1. The button is currently confirmed as pressed (debounced state is LOW).
    // 2. The 'hold' action has *not* already been triggered for this specific press event.
    // 3. Enough time has passed since the button was initially pressed (current time - press start time >= hold duration).
    if (buttonState == LOW && !buttonHeldTriggered && (currentMillis - buttonPressStartTime >= buttonHoldDuration))
    {
        // --- Long Press (Hold) Action ---
        buttonHeldTriggered = true; // Set the flag to prevent this action from repeating during the same hold.
        Serial.println("Entering SLEEP mode (Hold Detected).");
        isLowPowerMode = true; // Set the flag to indicate we are entering sleep (used to pause other loop activities).

        // Display a "SLEEPING..." message briefly before actually sleeping.
        displayCenteredMessage("SLEEPING...", 28);
        delay(sleepGraceTime); // Short delay to ensure the button is released before the board powers down so that it doesn't wake up immediately after sleeping.

        // Prepare for sleep: Turn off peripherals to save power.
        display.clearDisplay();      // Clear the display buffer.
        display.display();           // Send the clear command to the physical display (effectively turns it off).
        digitalWrite(LED_PIN, HIGH); // Turn off the indicator LED (assuming active LOW).
        ledIsOn = false;             // Reset the LED blink logic state tracker.

        // Optional: End Serial communication cleanly before sleep. Might save a tiny bit of power
        // and prevent garbled output on wake if the UART state isn't preserved perfectly.
        // Serial.end();

        // *** ENTER SLEEP MODE ***
        // Execution will pause here. The microcontroller enters the configured low-power state (SLEEP_MODE).
        // It will remain here until the wakeup interrupt (button release - RISING edge on PA0) occurs.
        LowPower.sleep();
        // *************************

        // --- Wake Up Sequence ---
        // Execution resumes HERE immediately after the wakeup interrupt (button release) is handled.
        isLowPowerMode = false; // We are now awake, clear the flag.

        // Re-initialize Serial communication as it might be disabled or in an unstable state after sleep.
        Serial.begin(9600);
        delay(100); // Small delay to allow Serial to re-establish connection if needed.
        Serial.println("\n--------------------------");
        Serial.println("Woke up from sleep!");
        Serial.println("--------------------------");

        // Ensure LED is off initially after wake-up.
        digitalWrite(LED_PIN, HIGH);
        ledIsOn = false; // Reset LED blink state tracker.

        // Force an immediate sensor read and display update on wake-up.
        // Do this by setting the 'last update' times far in the past relative to the current time.
        // This ensures the timing conditions `(currentMillis - last...Time >= interval)` will be met
        // in the next checks within this loop iteration.
        // Avoid setting directly to 0 if millis() might have wrapped or reset inaccurately during sleep.
        unsigned long now = millis();
        lastSensorReadTime = now - sensorReadInterval - 1;       // Ensure next sensor read happens now.
        lastDisplayUpdateTime = now - displayUpdateInterval - 1; // Ensure next display update happens now.

        // Note on button logic after wake-up:
        // The wake-up was triggered by the button *release* (RISING edge).
        // The `buttonState` will become HIGH shortly after wake-up (after debounce).
        // The `buttonHeldTriggered` flag is still TRUE from the hold action.
        // Therefore, the "Short Press Action" (cycling display) in the release logic above
        // will *not* execute immediately upon waking up, because `!buttonHeldTriggered` will be false.
        // This is generally the desired behavior: the long press to sleep/wake doesn't also trigger a mode change.
        // The *next* separate short press (press and release) will cycle the display mode.

        // Re-read the button state immediately after wake-up to ensure consistency.
        lastButtonState = digitalRead(BUTTON_PIN); // Update raw state tracker.
        buttonState = lastButtonState;             // Update debounced state tracker.
        // buttonPressStartTime is already handled by release logic.
        // buttonHeldTriggered will be reset on the *next* press (when buttonState goes LOW).
    }

    // 4. Save the current raw reading for the next loop iteration's change detection.
    lastButtonState = reading;
    // --- End Button Logic ---

    // --- Conditional Operations based on Power Mode ---
    // Only perform sensor reads, LED blinking, and display updates if the device
    // is NOT currently in the process of entering sleep mode.
    if (!isLowPowerMode)
    {
        // --- Timed Sensor Reading ---
        // Check if enough time has passed since the last sensor read.
        if (currentMillis - lastSensorReadTime >= sensorReadInterval)
        {
            lastSensorReadTime = currentMillis; // Update the timestamp for the last read.
            readSensors();                      // Call the function to read sensors, update cache, handle LED, print Teleplot.
        }

        // --- Non-blocking LED Blink OFF Logic ---
        // This handles turning the LED OFF after the short 'ledBlinkDuration' following a *successful* sensor read.
        // Check if:
        // 1. The 'ledIsOn' flag is true (meaning a successful read just occurred and the blink timer is active).
        // 2. Enough time has passed since the blink timer started (ledBlinkStartTime recorded in readSensors).
        // Use millis() directly here for slightly higher accuracy than currentMillis which was captured at the loop start.
        if (ledIsOn && (millis() - ledBlinkStartTime >= ledBlinkDuration))
        {
            digitalWrite(LED_PIN, HIGH); // Turn the LED OFF (assuming active LOW).
            ledIsOn = false;             // Deactivate the timed blink state; the LED will remain OFF until the next read cycle starts.
                                         // If the next read fails, the LED will turn ON and stay ON.
                                         // If the next read succeeds, the LED will turn ON, then this logic will turn it OFF again after the delay.
        }

        // --- Timed Display Update ---
        // Check if enough time has passed since the last display update.
        if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval)
        {
            lastDisplayUpdateTime = currentMillis; // Update the timestamp for the last display update.
            printSensorData();                     // Call the function to update the display based on the current mode and sensor cache.
        }
    }
    // --- End Conditional Operations ---

    // The loop() function implicitly repeats. Add a small delay if needed to yield time,
    // but the current structure relies on millis() timing and doesn't strictly require a delay here.
    // delay(1); // Optional small delay

} // End of loop()