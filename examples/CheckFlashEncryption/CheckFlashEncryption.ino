// Diagnostic sketch - NOT part of normal library usage.
//
// Checks the ACTUAL hardware eFuse state for flash encryption, rather than
// relying on what Arduino IDE's Tools menu says or what you remember
// configuring. Run this if you're seeing "Decryption error" on Serial
// during an OTA upload via SHN_OtaLib - it tells you definitively whether
// that's the cause.

#include "esp_flash_encryption.h"

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.print("Flash encryption enabled (hardware eFuse): ");
    Serial.println(esp_flash_encryption_enabled() ? "YES" : "NO");

    if (esp_flash_encryption_enabled())
    {
        Serial.println();
        Serial.println("This is almost certainly why OTA writes are failing");
        Serial.println("with 'Decryption error': the chip expects data written");
        Serial.println("to encrypted partitions to already be in the format its");
        Serial.println("hardware AES engine expects, and a plain .bin uploaded");
        Serial.println("over HTTP isn't. This is a chip-level security feature,");
        Serial.println("not something SHN_OtaLib (or any OTA library) controls.");
    }
}

void loop() {}
