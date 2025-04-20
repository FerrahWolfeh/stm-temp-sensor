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
const unsigned long ledBlinkDuration = 50;       // How long the LED stays on during blink

// --- Timing Variables ---
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
// --- Non-blocking LED Blink Variables ---
unsigned long ledBlinkStartTime = 0;
bool ledIsOn = false; // Tracks if the timed blink is active

// --- Debounce Variables ---
unsigned long lastDebounceTime = 0; // Time the button state last changed
unsigned long debounceDelay = 50;   // Debounce time (ms). 50ms is usually good.
byte buttonState = HIGH;            // Current processed button state (stable)
byte lastButtonState = HIGH;        // Previous raw reading from pin

// --- Display Mode Enum ---
enum DisplayMode
{
    PRESSURE_MBAR,
    PRESSURE_ATM,
    ALTITUDE,
    UPTIME // Added Uptime mode
};
const int NUM_DISPLAY_MODES = 4; // Total number of modes

// --- State Variables ---
DisplayMode currentDisplayMode = PRESSURE_MBAR; // Start with mBar display

// --- Global Sensor Value Cache ---
float currentTemperature = NAN;
float currentHumidity = NAN;
float currentPressureMbar = NAN;
float currentPressureAtm = NAN;
float currentAltitude = NAN;
bool sensorReadError = false; // Flag to indicate if the last read failed

// --- RTC Uptime Variable ---
time_t startTimeEpoch = 0; // Store the RTC epoch at the end of setup

// --- Peripheral Objects ---
Adafruit_BME280 bme;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
STM32RTC &rtc = STM32RTC::getInstance(); // Get the RTC instance

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
void printSensorData()
{
    // ... (function unchanged) ...
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);

    if (sensorReadError && currentDisplayMode != UPTIME)
    {
        displayCenteredText("BME Sensor Error!", 20);
        displayCenteredText("Check Connection", 35);
    }
    else
    {
        switch (currentDisplayMode)
        {
        case PRESSURE_MBAR:
            if (isnan(currentPressureMbar))
            {
                displayCenteredText("Reading...", 28);
            }
            else
            {
                display.setCursor(10, 10);
                display.print("Press: ");
                display.print(currentPressureMbar, 2);
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
                display.print(currentPressureAtm, 4);
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
                display.print(currentAltitude, 1);
                display.println(" m");
            }
            break;
        case UPTIME:
        {
            time_t currentEpoch = rtc.getEpoch();
            unsigned long totalSeconds = (currentEpoch >= startTimeEpoch) ? (currentEpoch - startTimeEpoch) : 0;
            int seconds = totalSeconds % 60;
            int minutes = (totalSeconds / 60) % 60;
            int hours = (totalSeconds / 3600) % 24;
            int days = totalSeconds / (3600 * 24);
            char uptimeStr[25];
            snprintf(uptimeStr, sizeof(uptimeStr), "%d d %02d:%02d:%02d", days, hours, minutes, seconds);
            displayCenteredText("Device Uptime", 15);
            displayCenteredText(uptimeStr, 35);
        }
        break;
        }

        if (currentDisplayMode != UPTIME)
        {
            if (isnan(currentTemperature) || isnan(currentHumidity))
            {
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
    display.display();
}

// --- Function to Read Sensors and Update Globals ---
void readSensors()
{
    digitalWrite(PC13, LOW); // Turn LED ON at the start of the read attempt
    // *** DEBUG LINE ***
    Serial.print(millis());
    Serial.println(" : LED ON");

    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float pres = bme.readPressure();

    if (isnan(temp) || isnan(hum) || isnan(pres))
    {
        Serial.println("Sensor read FAILED."); // DEBUG
        sensorReadError = true;
        ledIsOn = false; // Ensure timed blink logic doesn't turn off the solid error LED
    }
    else
    {
        // *** DEBUG LINE ***
        // Serial.println("Sensor read SUCCESS."); // Can comment this out if desired

        sensorReadError = false;
        currentTemperature = temp;
        currentHumidity = hum;
        currentPressureMbar = pres / 100.0F;
        currentPressureAtm = currentPressureMbar / 1013.25F;
        currentAltitude = bme.readAltitude(1013.25);

        // --- Teleplot Formatted Serial Output ---
        Serial.print(">Pressure (mBar):");
        Serial.print(currentPressureMbar, 2); // 2 decimal places
        Serial.println("§mBar");

        Serial.print(">Pressure (atm):");
        Serial.print(currentPressureAtm, 4); // 4 decimal places
        Serial.println("§atm");

        Serial.print(">Temperature:");
        Serial.print(currentTemperature, 1); // 1 decimal place
        Serial.println("§C");

        Serial.print(">Humidity:");
        Serial.print(currentHumidity, 1); // 1 decimal place
        Serial.println("§%");

        Serial.print(">Altitude:");
        Serial.print(currentAltitude, 1); // 1 decimal place
        Serial.println("§m");
        // --- End Teleplot Output ---

        // --- Start non-blocking LED blink timer ---
        ledIsOn = true;               // Activate the timed blink
        ledBlinkStartTime = millis(); // Record the start time
        // *** DEBUG LINE ***
        Serial.print(ledBlinkStartTime);
        Serial.println(" : Blink timer started");
    }
}

void setup()
{
    // ... (Setup remains the same) ...
    pinMode(PC13, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(PC13, HIGH);
    Serial.begin(9600);
    Wire.begin();
    Serial.println("Starting Initialization...");
    Serial.println("--------------------------");
    Serial.println("Initializing RTC...");
    rtc.setClockSource(STM32RTC::LSE_CLOCK);
    rtc.begin();
    delay(100);
    Serial.println("Setting RTC epoch to 0...");
    rtc.setEpoch(0);
    delay(100);
    startTimeEpoch = rtc.getEpoch();
    Serial.print("RTC setup complete. Start Epoch recorded: ");
    Serial.println(startTimeEpoch);
    Serial.println("--------------------------");
    Serial.println("Initializing Display...");
    while (!display.begin(0x3C, true))
    {
        Serial.println("SH1106 failed");
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
    Serial.println("Initializing BME280 Sensor...");
    while (!bme.begin(BME_ADDR, &Wire))
    {
        Serial.println("BME280 failed");
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
    Serial.println("Performing initial sensor read...");
    readSensors();
    if (sensorReadError)
    {
        Serial.println("Initial sensor read failed! LED may remain ON.");
        printSensorData();
    }
    else
    {
        Serial.println("Initial sensor read OK.");
    }
    Serial.println("Button handling uses polling.");
    Serial.println("--------------------------");
    display.clearDisplay();
    displayCenteredText("Ready!", 28);
    display.display();
    delay(1000);
    display.clearDisplay();
    display.display();
    Serial.println("Setup Complete. Entering main loop.");
    Serial.println("==========================");
    lastSensorReadTime = millis();
    lastDisplayUpdateTime = millis();
    lastButtonState = digitalRead(BUTTON_PIN);
}

void loop()
{
    unsigned long loopStartMillis = millis(); // Capture millis at the start

    // --- Button Debounce Logic (Polling State Machine) ---
    byte reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState)
    {
        lastDebounceTime = loopStartMillis; // Use loopStartMillis here too
    }
    // Use loopStartMillis for debounce check consistency within the loop iteration
    if ((loopStartMillis - lastDebounceTime) > debounceDelay)
    {
        if (reading != buttonState)
        {
            buttonState = reading;
            if (buttonState == LOW)
            {
                Serial.println("Button Pressed (Debounced)"); // Keep this debug line
                currentDisplayMode = (DisplayMode)(((int)currentDisplayMode + 1) % NUM_DISPLAY_MODES);
                // Serial.print("Display Mode: "); // Can comment this out if desired
                // switch (currentDisplayMode) { /* ... cases ... */ } // Can comment this out
                printSensorData();
                lastDisplayUpdateTime = loopStartMillis;
            }
        }
    }
    lastButtonState = reading;
    // --- End Button Debounce ---

    // --- Timed Sensor Reading ---
    if (loopStartMillis - lastSensorReadTime >= sensorReadInterval)
    {
        lastSensorReadTime = loopStartMillis; // Record time based on loop start
        readSensors();                        // Reads sensors and prints Teleplot data
    }

    // --- Non-blocking LED Blink OFF Logic ---
    unsigned long currentCheckMillis = millis();
    if (ledIsOn && (currentCheckMillis - ledBlinkStartTime >= ledBlinkDuration))
    {
        // *** DEBUG LINES use currentCheckMillis for consistency ***
        Serial.print(currentCheckMillis); // Print the time used for the check
        Serial.print(" : Turning LED OFF. StartTime=");
        Serial.print(ledBlinkStartTime);
        Serial.print(", Diff=");
        Serial.println(currentCheckMillis - ledBlinkStartTime);

        digitalWrite(PC13, HIGH); // Turn LED OFF
        ledIsOn = false;          // Deactivate timed blink
    }

    // --- Timed Display Update ---
    if (loopStartMillis - lastDisplayUpdateTime >= displayUpdateInterval)
    {
        lastDisplayUpdateTime = loopStartMillis; // Record time based on loop start
        printSensorData();
    }

} // End of loop()
