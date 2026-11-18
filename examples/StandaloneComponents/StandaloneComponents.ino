// Example: skip the OtaManager facade and wire WifiConnector + HttpOtaProvider
// together manually. Useful when a project (like a gateway device) already
// manages its own WiFi connection and main loop, and just wants to turn the
// OTA HTTP server on/off on demand - e.g. when a physical button is held,
// without OtaManager taking over WiFi bring-up too.

#include <WifiConnector.h>
#include <HttpOtaProvider.h>
#include <OtaCredentials.h>

WifiConnector wifi;                 // pretend this is already managed by the app
HttpOtaProvider* otaHttp = nullptr;

const int OTA_BUTTON_PIN = 0;       // active-low boot button, just as an example

void startOtaWindow()
{
    if (otaHttp) return;   // already running

    HttpOtaProvider::Config cfg;
    cfg.port          = 8080;
    cfg.user          = "admin";
    cfg.deviceId      = OtaCredentials::deviceId();
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.2.0";
    cfg.activeMinutes = 10;
    // cfg.password left empty -> auto-generated PIN

    otaHttp = new HttpOtaProvider(cfg);
    otaHttp->begin(wifi.ip());

    Serial.print("OTA window opened at: ");
    Serial.println(otaHttp->updateUrl());
    Serial.print("PIN: ");
    Serial.println(otaHttp->currentPassword());
}

void setup()
{
    Serial.begin(115200);
    pinMode(OTA_BUTTON_PIN, INPUT_PULLUP);

    // App owns WiFi bring-up; OTA classes don't require it to happen any
    // particular way - they just need an IPAddress once connected.
    wifi.begin("your-ssid", "your-password", "IP-BOX-01",
               WifiConnector::Mode::Station, /*timeoutSec=*/15);

    if (!wifi.isConnected())
    {
        Serial.println("WiFi not connected - OTA can be started later once it is.");
    }
}

void loop()
{
    // ... normal application work goes here, uninterrupted ...

    if (digitalRead(OTA_BUTTON_PIN) == LOW && wifi.isConnected())
    {
        startOtaWindow();
        delay(500);   // crude debounce
    }

    if (otaHttp)
    {
        if (!otaHttp->handle())
        {
            Serial.println("OTA window closed.");
            delete otaHttp;
            otaHttp = nullptr;
        }
    }
}
