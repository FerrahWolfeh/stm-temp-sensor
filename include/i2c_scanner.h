#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

/**
 * @brief Scans the I2C bus (addresses 1-126) and prints found devices to Serial.
 *        Requires Serial and Wire to be initialized beforehand.
 */
void scanI2Cbus();

#endif // I2C_SCANNER_H
