#include "i2c_scanner.h"
#include <Arduino.h> // For Serial, byte, HEX
#include <Wire.h>    // For I2C functions

/**
 * @brief Scans the I2C bus (addresses 1-126) and prints found devices to Serial.
 *        Requires Serial and Wire to be initialized beforehand.
 */
void scanI2Cbus()
{
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        // The i2c_scanner uses the return value of
        // Wire.endTransmission to see if a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) // Success, device acknowledged
        {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
            {
                Serial.print("0"); // Add leading zero for formatting
            }
            Serial.print(address, HEX);
            Serial.println(" !");
            nDevices++;
        }
        else if (error == 4) // Other error (e.g., bus error)
        {
            Serial.print("Unknown error at address 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
        // error == 2 (NACK on address) is the expected case for no device, so no print needed.
    }
    if (nDevices == 0)
    {
        Serial.println("No I2C devices found. Check wiring and pin definitions!");
    }
    else
    {
        Serial.print("Found ");
        Serial.print(nDevices);
        Serial.println(" device(s).");
    }
    Serial.println("I2C Scan Complete.");
    Serial.println("--------------------------");
}
