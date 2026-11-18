// Basic example: connect to WiFi and expose the HTTP OTA update page.
// Works unchanged on both ESP32 and ESP8266.
//
// NOTE: the default pages regenerate the PIN from a fresh random salt every
// time the root page ("/") is loaded (see README > "built-in pages and
// their endpoints"). onPasswordChanged below reprints the PIN whenever that
// happens, so Serial always shows the one that's actually valid.

#include <SHN_OtaLib.h>

// Optional: your secret pepper (see README > Security model). Copy
// tools/OtaSecrets.example.h next to this sketch as OtaSecrets.h.
#if __has_include("OtaSecrets.h")
#include "OtaSecrets.h"
#endif

OtaManager* ota;

void setup()
{
    Serial.begin(115200);

    OtaManager::Config cfg;
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.0.0";
    cfg.wifiSsid      = "your-ssid";
    cfg.wifiPassword  = "your-password";
    cfg.wifiConnectTimeoutSec = 15;
    cfg.activeMinutes = 10;     // OTA window closes after 10 minutes
#ifdef OTA_SECRET_PEPPER
    cfg.pepper        = OTA_SECRET_PEPPER;
#endif

    cfg.onPasswordChanged = [](const String& newPassword) {
        Serial.print("Update PIN changed, now: ");
        Serial.println(newPassword);
    };

    ota = new OtaManager(cfg);

    if (ota->begin())
    {
        Serial.print("OTA ready at: ");
        Serial.println(ota->updateUrl());
        Serial.print("Update PIN: ");
        Serial.println(ota->updatePassword());
    }
    else
    {
        Serial.println("WiFi connection failed, OTA not started.");
    }
}

void loop()
{
    if (ota && !ota->handle())
    {
        // OTA window closed - free WiFi/radio and resume normal operation.
        ota->end();
        delete ota;
        ota = nullptr;
    }

    // ... rest of the application's normal loop() work ...
}
