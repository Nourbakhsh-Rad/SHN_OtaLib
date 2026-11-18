// Example: OTA over Ethernet only - WiFi is never started, never even
// initialized. Useful when the board's only network path is Ethernet
// (LAN8720) and there's no reason to ever bring up a WiFi radio.
//
// Key difference from the other examples: instead of ota->begin() (which
// makes OtaManager bring up WiFi itself), we call the ota->begin(ip)
// overload once Ethernet is already connected. OtaManager then never
// touches WiFi - not to connect it, and not to turn it off later either.

#include <ETH.h>
#include <SHN_OtaLib.h>

// ---- adjust to match the IP-box board's LAN8720 wiring ----
#define ETH_ADDR        0
#define ETH_POWER_PIN   -1
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
// -------------------------------------------------------------

OtaManager* ota = nullptr;
bool ethReady = false;

void onEthEvent(WiFiEvent_t event)
{
    if (event == ARDUINO_EVENT_ETH_GOT_IP)
    {
        ethReady = true;
        Serial.print("Ethernet up, IP: ");
        Serial.println(ETH.localIP());

        if (!ota)
        {
            OtaManager::Config cfg;
            cfg.hardwareId    = "IP-BOX";
            cfg.swVersion     = "1.2.0";
            cfg.activeMinutes = 0;   // 0 = stay open for as long as the device runs

            cfg.onNetworkUp = [](IPAddress ip) {
                Serial.print("[OTA] network ready, service reachable at ");
                Serial.println(ip);
            };

            ota = new OtaManager(cfg);
            ota->begin(ETH.localIP());   // <-- overload: no WiFi involved at all

            Serial.print("OTA ready at: "); Serial.println(ota->updateUrl());
            Serial.print("PIN: ");          Serial.println(ota->updatePassword());
        }
    }
    else if (event == ARDUINO_EVENT_ETH_DISCONNECTED)
    {
        ethReady = false;
        Serial.println("Ethernet link down.");
    }
}

void setup()
{
    Serial.begin(115200);

    WiFi.onEvent(onEthEvent);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    // Note: no WiFi.mode(), no WiFi.begin() anywhere in this sketch.
}

void loop()
{
    if (ota) ota->handle();

    // ... normal gateway/application work (over Ethernet) goes here ...
}
