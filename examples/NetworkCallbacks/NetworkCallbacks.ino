// Example: react to the network coming up or dropping, when OtaManager is
// the one managing WiFi (the no-argument begin()). onNetworkUp fires once
// the OTA service is actually reachable; onNetworkDown fires if that WiFi
// connection is later detected as dropped (checked once per handle() call).
//
// Note: onNetworkDown only ever fires for a self-managed connection - it is
// never called when the network was handed in via begin(ip) (Ethernet, or
// WiFi the sketch connected itself), since OtaManager doesn't own or
// monitor that connection.

#include <SHN_OtaLib.h>

OtaManager* ota;
volatile bool ledShouldBlink = false;   // toggled by the down callback, read in loop()

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    OtaManager::Config cfg;
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.2.0";
    cfg.wifiSsid      = "your-ssid";
    cfg.wifiPassword  = "your-password";
    cfg.wifiConnectTimeoutSec = 15;
    cfg.activeMinutes = 0;

    cfg.onNetworkUp = [](IPAddress ip) {
        Serial.print("[net] up, OTA reachable at ");
        Serial.println(ip);
        ledShouldBlink = false;
        digitalWrite(LED_BUILTIN, LOW);
    };

    cfg.onNetworkDown = []() {
        Serial.println("[net] WiFi dropped - OTA unreachable until it reconnects.");
        ledShouldBlink = true;   // e.g. blink an LED to flag "network lost" locally
    };

    ota = new OtaManager(cfg);

    if (!ota->begin())
    {
        Serial.println("[net] initial WiFi connect failed.");
    }
}

void loop()
{
    if (ota) ota->handle();   // onNetworkDown is checked here, once per call

    // ... normal application work ...

    if (ledShouldBlink)
    {
        digitalWrite(LED_BUILTIN, (millis() / 250) % 2);
    }
}
