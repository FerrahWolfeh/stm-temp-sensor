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

Adafruit_BME280 bme;
// Use SH1106G for the 1.3" OLED
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Minimal ISR ---
void handleButtonInterrupt()
{
    buttonPressedFlag = true;
};

// Helper function to display centered text
void displayCenteredText(const char *text, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h); // Calculate bounds
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, y);
    display.print(text);
}

void printSensorData(float pressure, float temperature, float humidity)
{
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
    display.setCursor(10, 10);
    display.print("Press: ");
    display.print(pressure, 3);
    display.println(" atm");
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
    // Initialize Wire *before* display.begin()
    Wire.begin(); // Use default SDA/SCL pins defined by the board variant (PB7/PB6 for Blackpill)

    Serial.println("Starting Initialization...");
    Serial.println("--------------------------");

    // Initialize Display - Retry Loop
    Serial.println("Initializing Display...");
    while (!display.begin(0x3C, true)) // Address 0x3C, init i2c if not yet started
    {
        Serial.println(F("SH1106 connection failed."));
        Serial.print(F("Expecting SCL=PB"));
        Serial.print(PIN_WIRE_SCL); // Use defined pin number from variant
        Serial.print(F(", SDA=PB"));
        Serial.print(PIN_WIRE_SDA); // Use defined pin number from variant
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
    displayCenteredText("Display OK", 28); // Use helper function
    display.display();
    delay(500); // Show message briefly
    Serial.println("--------------------------");

    // --- Initialize BME280 - Retry Loop ---
    Serial.println("Initializing BME280 Sensor...");
    display.clearDisplay();
    displayCenteredText("Finding BME280...", 28); // Use helper function
    display.display();
    while (!bme.begin(BME_ADDR, &Wire)) // Pass Wire object explicitly
    {
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
        displayCenteredText("BME280 Not Found!", 5);
        displayCenteredText("Check Wiring:", 20);
        displayCenteredText("SDA=PB7, SCL=PB6", 35);
        displayCenteredText("Retrying...", 50);
        display.display();

        digitalWrite(PC13, LOW);
        delay(500);
        digitalWrite(PC13, HIGH);
        delay(500);
    }
    // --- BME280 Initialized OK ---
    Serial.println("BME280 Found!");
    display.clearDisplay();
    char bmeOkMsg[20]; // Buffer for the message
    snprintf(bmeOkMsg, sizeof(bmeOkMsg), "BME280 OK (0x%X)", BME_ADDR);
    displayCenteredText(bmeOkMsg, 28); // Use helper function
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
    displayCenteredText("Ready!", 28); // Use helper function
    display.display();
    delay(1000);            // Show message
    display.clearDisplay(); // Clear screen before entering loop
    display.display();      // Send clear command

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

                // --- Display "Bye" message ---
                display.clearDisplay();
                display.setTextSize(1);             // Ensure text size is set
                display.setTextColor(SH110X_WHITE); // Ensure color is set
                displayCenteredText("Bye ;)", 28);  // Display centered message
                display.display();
                delay(500); // Show message for 0.5 seconds
                // --- End "Bye" message ---

                // 1. Clear the display buffer again (good practice before power off)
                display.clearDisplay();
                // 2. Send the clear command to the physical display
                display.display();
                // 3. Wait a short moment for the command to process before cutting power
                delay(50); // Keep short delay
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
                delay(100); // Keep delay
                // 3. Re-initialize the display (important after power cycle)
                if (!display.begin(0x3C, true))
                {
                    Serial.println("Failed to re-init display after power on!");
                    next_screen_state = false;       // Force state back to off if re-init fails
                    digitalWrite(SCREEN_POWER, LOW); // Turn power back off
                }
                else
                {
                    // --- Display "Hello" message ---
                    display.clearDisplay();
                    display.setTextSize(1);              // Ensure text size is set
                    display.setTextColor(SH110X_WHITE);  // Ensure color is set
                    displayCenteredText("Hello ;P", 28); // Display centered message
                    display.display();
                    delay(500); // Show message for 0.5 seconds
                    // --- End "Hello" message ---

                    // Clear display buffer before resuming normal operation
                    display.clearDisplay();
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
                display.clearDisplay(); // Clear after successful re-init
                display.display();
                // Optionally show "Hello" again here if desired after unexpected power loss recovery
                // displayCenteredText("Hello ;P", 28);
                // display.display();
                // delay(500);
                // display.clearDisplay();
                // display.display();
            }
        }

        // Read sensor values
        float temperature = bme.readTemperature();
        float humidity = bme.readHumidity();
        float pressure = bme.readPressure() / 101325.0F; // Convert Pa to atm

        // Check for valid readings
        if (isnan(temperature) || isnan(humidity) || isnan(pressure))
        {
            Serial.println("Failed to read from BME sensor!");
            // Display sensor error message
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            displayCenteredText("BME Sensor Error!", 20);
            displayCenteredText("Check Connection", 35);
            display.display();
            // Still blink LED even if sensor fails, shows MCU is running
        }
        else
        {
            // Print to display
            printSensorData(pressure, temperature, humidity);

            // Print sensor values to serial monitor
            Serial.print(">Pressure:");
            Serial.print(pressure, 5);
            Serial.println("§atm"); // Corrected unit symbol
            Serial.print(">Temp:");
            Serial.print(temperature, 1);
            Serial.println("§C"); // Corrected unit symbol
            Serial.print(">Hum:");
            Serial.print(humidity, 1);
            Serial.println("§%"); // Corrected unit symbol
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
