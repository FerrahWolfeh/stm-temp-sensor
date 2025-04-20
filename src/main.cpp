#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <stdio.h>    // Required for snprintf
#include <STM32RTC.h> // Required for RTC

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define BME_ADDR 0x76
#define BUTTON_PIN PA0
#define I2C_SDA PB7 // Define I2C SDA pin (already default for Blackpill)
#define I2C_SCL PB6 // Define I2C SCL pin (already default for Blackpill)

// --- Timing Intervals (milliseconds) ---
const unsigned long sensorReadInterval = 1000;   // How often to read the sensor
const unsigned long displayUpdateInterval = 100; // How often to update the display (faster)
// Increased LED duration for better visibility as requested
const unsigned long ledBlinkDuration = 50; // How long the LED stays on during blink

// --- Timing Variables ---
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
// Removed LED timing variables, simplified logic below

// --- Debounce Variables ---
volatile bool buttonPressedFlag = false; // Flag set by ISR
unsigned long lastDebounceTime = 0;      // Last time the output pin was toggled
unsigned long debounceDelay = 100;       // Debounce time in milliseconds

// --- Display Mode Enum ---
enum DisplayMode
{
    PRESSURE_MBAR,
    PRESSURE_ATM,
    ALTITUDE,
    UPTIME // Added Uptime mode
};
// Updated number of modes
const int NUM_DISPLAY_MODES = 4; // Total number of modes

// --- State Variables ---
DisplayMode currentDisplayMode = PRESSURE_MBAR; // Start with mBar display

// --- Global Sensor Value Cache ---
// Store the latest valid sensor readings here
float currentTemperature = NAN;
float currentHumidity = NAN;
float currentPressureMbar = NAN;
float currentPressureAtm = NAN;
float currentAltitude = NAN;
bool sensorReadError = false; // Flag to indicate if the last read failed

Adafruit_BME280 bme;
// Use SH1106G for the 1.3" OLED
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Minimal ISR ---
void handleButtonInterrupt()
{
    // Simple flag setting, debounce handled in loop()
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

// --- Modified printSensorData ---
// Reads from global variables and uses the current display mode
void printSensorData()
{
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);

    // Check if the last sensor read failed *and* we are in a sensor display mode
    if (sensorReadError && currentDisplayMode != UPTIME)
    {
        // Display sensor error message only if relevant to the current screen
        displayCenteredText("BME Sensor Error!", 20);
        displayCenteredText("Check Connection", 35);
    }
    // Display data based on the current mode
    else
    {
        switch (currentDisplayMode)
        {
        case PRESSURE_MBAR:
            if (isnan(currentPressureMbar))
            { // Check specific value needed
                displayCenteredText("Reading...", 28);
            }
            else
            {
                display.setCursor(10, 10);
                display.print("Press: ");
                display.print(currentPressureMbar, 2); // Pressure in mBar, 2 decimal places
                display.println(" mBar");
            }
            break;
        case PRESSURE_ATM:
            if (isnan(currentPressureAtm))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                display.setCursor(10, 10);
                display.print("Press: ");
                display.print(currentPressureAtm, 4); // Pressure in atm, 4 decimal places
                display.println(" atm");
            }
            break;
        case ALTITUDE:
            if (isnan(currentAltitude))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                display.setCursor(10, 10);
                display.print("Alt: ");
                display.print(currentAltitude, 1); // Altitude in meters, 1 decimal place
                display.println(" m");
            }
            break;
        case UPTIME:
        { // Use braces to create a scope for local variables
            unsigned long totalMillis = millis();
            unsigned long totalSeconds = totalMillis / 1000;
            int seconds = totalSeconds % 60;
            int minutes = (totalSeconds / 60) % 60;
            int hours = (totalSeconds / 3600) % 24;
            int days = totalSeconds / (3600 * 24);

            char uptimeStr[25]; // Buffer for D d HH:MM:SS format
            // Format the string, ensuring leading zeros for H, M, S
            snprintf(uptimeStr, sizeof(uptimeStr), "%d d %02d:%02d:%02d", days, hours, minutes, seconds);

            displayCenteredText("Device Uptime", 15); // Title for the screen
            displayCenteredText(uptimeStr, 35);       // Display the formatted time
        } // End of UPTIME scope
        break;
        }

        // Display Temperature and Humidity only if NOT in Uptime mode and data is valid
        if (currentDisplayMode != UPTIME)
        {
            if (isnan(currentTemperature) || isnan(currentHumidity))
            {
                // Optionally show placeholders if temp/hum aren't ready yet
                display.setCursor(10, 30);
                display.println("Temp: --- C");
                display.setCursor(10, 50);
                display.println("Hum:  --- %");
            }
            else
            {
                display.setCursor(10, 30);
                display.print("Temp: ");
                display.print(currentTemperature, 1);
                display.println(" C");
                display.setCursor(10, 50);
                display.print("Hum: ");
                display.print(currentHumidity, 1);
                display.println(" %");
            }
        }
    }

    display.display(); // Update the physical display
}

// --- Function to Read Sensors and Update Globals ---
void readSensors()
{
    // --- LED Heartbeat Start ---
    // Turn LED ON at the *start* of the read attempt
    digitalWrite(PC13, LOW);

    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float pres = bme.readPressure(); // Read in Pascals (Pa)

    if (isnan(temp) || isnan(hum) || isnan(pres))
    {
        Serial.println("Failed to read from BME sensor!");
        sensorReadError = true;
        // Don't update globals if read failed, keep previous values
        // LED will stay ON until the next *successful* read attempt's delay finishes
    }
    else
    {
        sensorReadError = false; // Clear error flag on successful read
        currentTemperature = temp;
        currentHumidity = hum;
        currentPressureMbar = pres / 100.0F;                 // Convert Pa to hPa (mBar)
        currentPressureAtm = currentPressureMbar / 1013.25F; // Convert mBar to atm
        // Calculate altitude using standard sea level pressure (1013.25 hPa)
        // Note: readAltitude uses the pressure reading *from the sensor* at the time it's called.
        // For consistency, it might be better to calculate from our cached 'pres' value,
        // but Adafruit_BME280::readAltitude(seaLevel) likely just calls readPressure() again internally.
        // Using the library function is convenient.
        currentAltitude = bme.readAltitude(1013.25);

        // Print sensor values to serial monitor
        Serial.print(">Pressure:");
        Serial.print(currentPressureMbar, 2);
        Serial.println(" mBar");
        Serial.print(">Pressure:");
        Serial.print(currentPressureAtm, 4);
        Serial.println(" atm");
        Serial.print(">Temp:");
        Serial.print(currentTemperature, 1);
        Serial.println(" C");
        Serial.print(">Hum:");
        Serial.print(currentHumidity, 1);
        Serial.println(" %");
        Serial.print(">Altitude:");
        Serial.print(currentAltitude, 1);
        Serial.println(" m");
        Serial.println("----------");

        // --- LED Heartbeat End ---
        // Keep LED ON for the specified duration *after* a successful read
        delay(ledBlinkDuration);  // Blocking delay is acceptable here as it's short
        digitalWrite(PC13, HIGH); // Turn LED OFF
    }
    // If sensor read failed, the LED remains ON (solid) indicating an error state
    // until the next read attempt cycle.
}

void setup()
{
    pinMode(PC13, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(PC13, HIGH); // Turn off built-in LED initially

    Serial.begin(9600);
    Wire.begin(); // Use default SDA/SCL pins

    Serial.println("Starting Initialization...");
    Serial.println("--------------------------");
    Serial.println("Note: STM32F411CE has a hardware RTC, but uptime is tracked using millis().");
    Serial.println("RTC requires specific setup (clock source, optional VBAT) for wall-clock time.");
    Serial.println("--------------------------");

    // Initialize Display - Retry Loop
    Serial.println("Initializing Display...");
    while (!display.begin(0x3C, true))
    {
        Serial.println(F("SH1106 connection failed. Retrying..."));
        digitalWrite(PC13, LOW);
        delay(500);
        digitalWrite(PC13, HIGH);
        delay(500);
    }
    Serial.println("Display Initialized OK.");
    display.display();
    delay(100);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    displayCenteredText("Display OK", 28);
    display.display();
    delay(500);
    Serial.println("--------------------------");

    // Initialize BME280 - Retry Loop
    Serial.println("Initializing BME280 Sensor...");
    display.clearDisplay();
    displayCenteredText("Finding BME280...", 28);
    display.display();
    while (!bme.begin(BME_ADDR, &Wire))
    {
        Serial.println(F("BME280 connection failed. Retrying..."));
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        displayCenteredText("BME280 Not Found!", 5);
        displayCenteredText("Check Wiring", 20);
        displayCenteredText("Retrying...", 50);
        display.display();
        digitalWrite(PC13, LOW);
        delay(500);
        digitalWrite(PC13, HIGH);
        delay(500);
    }
    Serial.println("BME280 Found!");
    display.clearDisplay();
    char bmeOkMsg[20];
    snprintf(bmeOkMsg, sizeof(bmeOkMsg), "BME280 OK (0x%X)", BME_ADDR);
    displayCenteredText(bmeOkMsg, 28);
    display.display();
    delay(1000);
    Serial.println("--------------------------");

    // --- Perform Initial Sensor Read ---
    Serial.println("Performing initial sensor read...");
    // Don't turn LED off immediately after initial read in setup
    digitalWrite(PC13, LOW); // Turn LED on for initial read attempt
    readSensors();           // Populate global variables before first display
    // readSensors() now handles turning the LED off after delay *if successful*
    if (sensorReadError)
    {
        Serial.println("Initial sensor read failed! LED will remain ON.");
        // Display error immediately
        printSensorData(); // Will show error message based on flag/NAN
        delay(2000);       // Show error for a bit
        // LED stays ON because readSensors didn't turn it off
    }
    else
    {
        Serial.println("Initial sensor read OK.");
        // LED was turned off by readSensors() after its delay
    }

    // --- Attach Interrupt ---
    Serial.println("Attaching button interrupt...");
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);
    Serial.println("Interrupt attached.");
    Serial.println("--------------------------");

    // --- Ready Message ---
    display.clearDisplay();
    displayCenteredText("Ready!", 28);
    display.display();
    delay(1000);
    display.clearDisplay();
    display.display(); // Clear before loop

    Serial.println("Setup Complete. Entering main loop.");
    Serial.println("==========================");
    lastSensorReadTime = millis(); // Initialize timers
    lastDisplayUpdateTime = millis();
}

void loop()
{
    unsigned long currentMillis = millis();

    // --- Button Debounce Logic ---
    if (buttonPressedFlag)
    {
        if ((currentMillis - lastDebounceTime) > debounceDelay)
        {
            lastDebounceTime = currentMillis; // Reset debounce timer

            // Cycle through display modes
            currentDisplayMode = (DisplayMode)(((int)currentDisplayMode + 1) % NUM_DISPLAY_MODES);

            Serial.print("Button Pressed! Display Mode: ");
            switch (currentDisplayMode)
            {
            case PRESSURE_MBAR:
                Serial.println("Pressure (mBar)");
                break;
            case PRESSURE_ATM:
                Serial.println("Pressure (atm)");
                break;
            case ALTITUDE:
                Serial.println("Altitude (m)");
                break;
            case UPTIME:
                Serial.println("Uptime");
                break; // Added Uptime case
            }

            // --- Force immediate display update on button press ---
            printSensorData();
            lastDisplayUpdateTime = currentMillis; // Reset display timer to prevent immediate double update
        }
        // Reset the interrupt flag
        buttonPressedFlag = false;
    }

    // --- Timed Sensor Reading ---
    if (currentMillis - lastSensorReadTime >= sensorReadInterval)
    {
        lastSensorReadTime = currentMillis;
        readSensors(); // Read sensors, update globals, handle LED blink
    }

    // --- Timed Display Update ---
    // Update display periodically, more frequently than sensor reads
    if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval)
    {
        // Avoid updating display immediately after a button press already updated it
        // Check if enough time has passed *since the last update*, regardless of source
        lastDisplayUpdateTime = currentMillis;
        printSensorData();
    }

    // --- Non-blocking LED Management Removed ---
    // LED is now handled synchronously within readSensors()

} // End of loop()
