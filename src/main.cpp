#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define BME_ADDR 0x76
#define BUTTON_PIN PA0
#define SCREEN_POWER PB1 // Use a define for the screen power pin
#define I2C_SDA PB7      // Define I2C SDA pin
#define I2C_SCL PB6      // Define I2C SCL pin

// --- Debounce Variables ---
volatile bool buttonPressedFlag = false; // Flag set by ISR
unsigned long lastDebounceTime = 0;      // Last time the output pin was toggled
unsigned long debounceDelay = 50;        // Debounce time in milliseconds

// --- State Variables ---
bool screen_on = true; // Let's use positive logic: screen_on = true means display is active
// Removed led_state as it should always mirror screen_on for the power pin

Adafruit_BME280 bme;
// Use SH1106G for the 1.3" OLED
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Minimal ISR ---
void handleButtonInterrupt()
{
    buttonPressedFlag = true;
};

void printSensorData(float pressure, float temperature, float humidity)
{
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
    display.setCursor(10, 10);
    display.print("Press: ");
    display.print(pressure, 1);
    display.println(" mBar");
    display.setCursor(10, 30);
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");
    display.setCursor(10, 50);
    display.print("Hum: ");
    display.print(humidity, 1);
    display.println(" %");
    display.display();
}

void setup()
{
    pinMode(PC13, OUTPUT);
    pinMode(SCREEN_POWER, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(PC13, HIGH);                           // Turn off built-in LED initially
    digitalWrite(SCREEN_POWER, screen_on ? HIGH : LOW); // Set initial power based on screen_on state

    Serial.begin(9600);
    Wire.begin();

    Serial.println("Starting Initialization...");
    Serial.println("--------------------------");

    // Initialize Display - Retry Loop
    Serial.println("Initializing Display...");
    while (!display.begin(0x3C, true))
    {
        Serial.println(F("SH1106 connection failed."));
        Serial.print(F("Expecting SCL=PB"));
        Serial.print(PIN_WIRE_SCL);
        Serial.print(F(", SDA=PB"));
        Serial.print(PIN_WIRE_SDA);
        Serial.print(F(", Power=PB"));
        Serial.print(digitalPinToPinName(SCREEN_POWER));
        Serial.println(F(". Check wiring."));
        Serial.println(F("Retrying in 1 second..."));
        digitalWrite(PC13, LOW);
        delay(500);
        digitalWrite(PC13, HIGH);
        delay(500);
    }
    Serial.println("Display Initialized OK.");
    display.display(); // Show initial buffer (might be garbage)
    delay(100);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    // Center "Display OK" (10 chars * 6 pixels/char = 60 pixels wide)
    // X = (128 - 60) / 2 = 34
    display.setCursor(34, 28);   // Centered X, Middle Y
    display.print("Display OK"); // Use print for centering
    display.display();
    delay(500); // Show message briefly
    Serial.println("--------------------------");

    // --- Initialize BME280 - Retry Loop ---
    Serial.println("Initializing BME280 Sensor...");
    display.clearDisplay();
    // Center "Finding BME280..." (17 chars * 6 pixels/char = 102 pixels wide)
    // X = (128 - 102) / 2 = 13
    display.setCursor(13, 28);          // Centered X, Middle Y
    display.print("Finding BME280..."); // Use print for centering
    display.display();
    while (!bme.begin(BME_ADDR))
    {
        // ... (BME error handling code remains the same - already centered) ...
        Serial.println(F("BME280 connection failed."));
        Serial.print(F("Expecting SCL=PB"));
        Serial.print(PIN_WIRE_SCL);
        Serial.print(F(", SDA=PB"));
        Serial.print(PIN_WIRE_SDA);
        Serial.print(F(" (Address 0x"));
        Serial.print(BME_ADDR, HEX);
        Serial.println(F("). Check wiring."));
        Serial.println(F("Retrying in 1 second..."));

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 5);
        display.print("BME280 Not Found!");
        display.setCursor(25, 25);
        display.print("Check Wiring:");
        display.setCursor(13, 35);
        display.print("SDA=PB7, SCL=PB6");
        display.setCursor(34, 50);
        display.print("Retrying...");
        display.display();

        digitalWrite(PC13, LOW);
        delay(500);
        digitalWrite(PC13, HIGH);
        delay(500);
    }
    // --- BME280 Initialized OK ---
    Serial.println("BME280 Found!");
    display.clearDisplay();
    // Center "BME280 OK (0xHH)" (17 chars * 6 pixels/char = 102 pixels wide)
    // X = (128 - 102) / 2 = 13
    display.setCursor(13, 28); // Centered X, Middle Y
    display.print("BME280 OK (0x");
    display.print(BME_ADDR, HEX);
    display.print(")"); // Use print for centering
    display.display();
    delay(1000); // Show message
    Serial.println("--------------------------");

    // --- Attach Interrupt ---
    Serial.println("Attaching button interrupt...");
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);
    Serial.println("Interrupt attached.");
    Serial.println("--------------------------");

    // --- Ready Message ---
    display.clearDisplay();
    // Center "Ready!" (6 chars * 6 pixels/char = 36 pixels wide)
    // X = (128 - 36) / 2 = 46
    display.setCursor(46, 28); // Centered X, Middle Y
    display.print("Ready!");   // Use print for centering
    display.display();
    delay(1000);            // Show message
    display.clearDisplay(); // Clear screen before entering loop

    Serial.println("Setup Complete. Entering main loop.");
    Serial.println("==========================");
}

void loop()
{
    // --- Button Debounce Logic ---
    if (buttonPressedFlag)
    {
        if ((millis() - lastDebounceTime) > debounceDelay)
        {
            Serial.println("Button Pressed!");
            lastDebounceTime = millis();

            // Store the *intended* next state
            bool next_screen_state = !screen_on;

            if (next_screen_state == false) // Turning OFF
            {
                Serial.println("Screen turning OFF");
                // 1. Clear the display buffer
                display.clearDisplay();
                // 2. Send the clear command to the physical display
                display.display();
                // 3. Wait a short moment for the command to process before cutting power
                delay(50); // Increased delay slightly, adjust if needed
                // 4. Now, cut the power
                digitalWrite(SCREEN_POWER, LOW);
                Serial.println("Screen Power Pin (PB1) set to: LOW (OFF)");
            }
            else // Turning ON
            {
                Serial.println("Screen turning ON");
                // 1. Apply power first
                digitalWrite(SCREEN_POWER, HIGH);
                Serial.println("Screen Power Pin (PB1) set to: HIGH (ON)");
                // 2. Give power time to stabilize and display to wake up
                delay(100); // Increased delay, adjust if needed
                // 3. Re-initialize the display (important after power cycle)
                if (!display.begin(0x3C, true))
                {
                    Serial.println("Failed to re-init display after power on!");
                    // Optional: handle re-init failure, maybe revert state?
                    // For now, we'll proceed, but it might not display correctly.
                    next_screen_state = false;       // Force state back to off if re-init fails
                    digitalWrite(SCREEN_POWER, LOW); // Turn power back off
                }
                else
                {
                    display.clearDisplay(); // Clear after successful re-init
                    display.display();
                }
            }

            // Update the master state variable *after* actions are taken
            screen_on = next_screen_state;
        }
        // Reset the interrupt flag regardless of debounce check
        buttonPressedFlag = false;
    }

    // --- Sensor Reading, Display Update, and Heartbeat ---
    if (screen_on)
    {
        // Ensure display power is ON (Safety check, though button logic should handle it)
        if (digitalRead(SCREEN_POWER) == LOW)
        {
            Serial.println("WARN: Screen power was LOW when screen_on was true. Turning ON.");
            digitalWrite(SCREEN_POWER, HIGH);
            delay(100); // Delay for power up
            // Attempt re-initialization again if power was unexpectedly off
            if (!display.begin(0x3C, true))
            {
                Serial.println("FATAL: Failed to re-init display after unexpected power loss!");
                screen_on = false;               // Mark screen as off
                digitalWrite(SCREEN_POWER, LOW); // Turn power back off
                // Skip the rest of the 'if (screen_on)' block for this iteration
                goto end_of_loop; // Use goto sparingly, but useful here to skip processing
            }
            else
            {
                display.clearDisplay();
                display.display();
            }
        }

        // Read sensor values
        float temperature = bme.readTemperature();
        float humidity = bme.readHumidity();
        float pressure = bme.readPressure() / 100.0F; // Convert Pa to hPa (mBar)

        // Check for valid readings
        if (isnan(temperature) || isnan(humidity) || isnan(pressure))
        {
            Serial.println("Failed to read from BME sensor!");
            // Display sensor error message
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            display.setCursor(10, 20);
            display.print("BME Sensor Error!");
            display.setCursor(10, 35);
            display.print("Check Connection");
            display.display();
            // Still blink LED even if sensor fails, shows MCU is running
        }
        else
        {
            // Print to display
            printSensorData(pressure, temperature, humidity);

            // Print sensor values to serial monitor
            Serial.print(">Pressure:");
            Serial.print(pressure, 1);
            Serial.println("§mBar");
            Serial.print(">Temp:");
            Serial.print(temperature, 1);
            Serial.println("§C");
            Serial.print(">Hum:");
            Serial.print(humidity, 1);
            Serial.println("§%");
            // Serial.println(" %");
        }

        // Blink the built-in LED as a heartbeat ONLY when screen is on
        digitalWrite(PC13, LOW);
        delay(50);
        digitalWrite(PC13, HIGH);

        // Main loop delay when screen is ON (adjust sensor update rate here)
        delay(950); // Total loop time approx 1000ms (950 + 50 for LED)
    }
    else // screen_on is false
    {
        // Ensure display power is OFF (Safety check)
        if (digitalRead(SCREEN_POWER) == HIGH)
        {
            Serial.println("WARN: Screen power was HIGH when screen_on was false. Turning OFF.");
            digitalWrite(SCREEN_POWER, LOW);
        }

        // Ensure LED is OFF when screen is off
        digitalWrite(PC13, HIGH); // HIGH turns PC13 LED OFF on many boards

        // Add a small delay when screen is off to prevent busy-waiting
        // and reduce CPU usage, while still being responsive to button.
        delay(100);
    }

end_of_loop:; // Label for the goto statement

} // End of loop()
