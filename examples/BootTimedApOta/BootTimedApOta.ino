// Scenario: the board's NORMAL network path is Ethernet (LAN8720 via RMII) -
// it never joins any WiFi network in regular operation. On every boot, it
// opens its own WiFi Access Point + HTTP OTA server for a fixed window
// (e.g. 5 minutes) so a technician can flash new firmware if needed, then
// automatically shuts the AP and OTA server down and continues running on
// Ethernet only, with WiFi fully off.
//
// WiFi (AP mode) and Ethernet run on independent hardware paths on ESP32,
// so they coexist fine - starting the AP does not interfere with the
// Ethernet link.
//
// Fill in the RMII pin numbers/clock mode for your LAN8720 wiring below.

#include <ETH.h>
#include <SHN_OtaLib.h>

// ---- adjust to match the IP-box board's LAN8720 wiring ----
#define ETH_ADDR        0
#define ETH_POWER_PIN   -1     // -1 if not used
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
// -------------------------------------------------------------

static bool ethConnected = false;

OtaManager* ota = nullptr;
const uint32_t OTA_WINDOW_MINUTES = 5;   // <-- the "fixed time" the AP+OTA stay up

void onEthEvent(WiFiEvent_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_ETH_GOT_IP:
            ethConnected = true;
            Serial.print("Ethernet up, IP: ");
            Serial.println(ETH.localIP());
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            ethConnected = false;
            Serial.println("Ethernet link down.");
            break;
        default:
            break;
    }
}

void startBootOtaWindow()
{
    OtaManager::Config cfg;
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.2.0";

    cfg.asAccessPoint = true;
    cfg.wifiSsid      = "IPBOX-SETUP";
    cfg.wifiPassword  = "";              // open AP; set a password if you prefer

    cfg.activeMinutes = OTA_WINDOW_MINUTES;

    ota = new OtaManager(cfg);

    if (ota->begin())
    {
        Serial.println("[Boot OTA] Access Point + OTA window opened.");
        Serial.print("[Boot OTA] Connect to WiFi: "); Serial.println(cfg.wifiSsid);
        Serial.print("[Boot OTA] Then open: ");        Serial.println(ota->updateUrl());
        Serial.print("[Boot OTA] PIN: ");              Serial.println(ota->updatePassword());
        Serial.print("[Boot OTA] Closes automatically in ");
        Serial.print(OTA_WINDOW_MINUTES);
        Serial.println(" minute(s).");
    }
    else
    {
        Serial.println("[Boot OTA] Failed to start Access Point.");
        delete ota;
        ota = nullptr;
    }
}

void setup()
{
    Serial.begin(115200);

    // Ethernet: the board's normal, permanent network path.
    WiFi.onEvent(onEthEvent);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

    // WiFi AP + OTA: only for the fixed window right after boot.
    startBootOtaWindow();
}

void loop()
{
    if (ota)
    {
        if (!ota->handle())
        {
            Serial.println("[Boot OTA] Window expired - turning AP/OTA off.");
            ota->end();          // stops the HTTP server and switches WiFi off
            delete ota;
            ota = nullptr;
            Serial.println("[Boot OTA] Back to Ethernet-only operation.");
        }
    }

    // ... normal gateway/application work (over Ethernet) goes here,
    //     regardless of whether the OTA window is still open ...
}
