// Example: serve custom-branded OTA pages from LittleFS instead of the
// built-in minimal pages. Upload the files in this example's data/ folder
// to the board's LittleFS partition first (Arduino IDE: "ESP32/ESP8266
// LittleFS Data Upload" tool, or PlatformIO: `pio run -t uploadfs`).

#include <SHN_OtaLib.h>

FsPageProvider pageProvider;   // reads /index_root.html, /index_upload.html,
                                // /index_notfound.html from LittleFS
OtaManager* ota;

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed - check that data/ was uploaded.");
    }

    OtaManager::Config cfg;
    cfg.hardwareId   = "IP-BOX";
    cfg.swVersion    = "1.2.0";
    cfg.wifiSsid     = "your-ssid";
    cfg.wifiPassword = "your-password";
    cfg.wifiConnectTimeoutSec = 15;
    cfg.activeMinutes = 15;
    cfg.pageProvider = &pageProvider;   // <-- swap in the FS-backed provider

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
        Serial.println("WiFi connection failed.");
    }
}

void loop()
{
    if (ota && !ota->handle())
    {
        ota->end();
        delete ota;
        ota = nullptr;
        Serial.println("OTA window closed.");
    }
}
