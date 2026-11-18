// Full-featured example: tries to join the site WiFi network first; if that
// fails within the timeout, falls back to its own Access Point so a
// technician can still reach it. Serves custom-branded pages from LittleFS
// and accepts firmware both via the HTTP page and via FTP.
//
// Upload this example's data/ folder to LittleFS before flashing, same as
// the FilesystemPages example.

#define OTA_ENABLE_FTP
#include <SHN_OtaLib.h>

FsPageProvider pageProvider;
OtaManager* ota = nullptr;

const char* SITE_SSID     = "site-wifi";
const char* SITE_PASSWORD = "site-password";
const char* SETUP_AP_SSID = "IPBOX-SETUP";

bool startOta(bool asAccessPoint)
{
    OtaManager::Config cfg;
    cfg.hardwareId   = "IP-BOX";
    cfg.swVersion    = "1.2.0";
    cfg.pageProvider = &pageProvider;
    cfg.activeMinutes = 20;

    cfg.enableFtp   = true;
    cfg.ftpUser     = "admin";
    cfg.ftpPassword = "change-me";
    cfg.ftpDoneCallback = [](bool success) {
        Serial.println(success ? "[FTP] firmware flashed OK."
                                : "[FTP] firmware flash FAILED.");
    };

    if (asAccessPoint)
    {
        cfg.asAccessPoint = true;
        cfg.wifiSsid      = SETUP_AP_SSID;
        cfg.wifiPassword  = "";   // open AP for easy field access
    }
    else
    {
        cfg.wifiSsid              = SITE_SSID;
        cfg.wifiPassword          = SITE_PASSWORD;
        cfg.wifiConnectTimeoutSec = 15;
    }

    ota = new OtaManager(cfg);

    if (!ota->begin())
    {
        delete ota;
        ota = nullptr;
        return false;
    }

    Serial.println(asAccessPoint ? "[OTA] Started as Access Point."
                                  : "[OTA] Joined site WiFi.");
    Serial.print("[OTA] HTTP: ");   Serial.println(ota->updateUrl());
    Serial.print("[OTA] PIN:  ");   Serial.println(ota->updatePassword());
    return true;
}

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed - custom pages won't load.");
    }

    if (!startOta(/*asAccessPoint=*/false))
    {
        Serial.println("[OTA] Site WiFi join failed, falling back to Access Point.");
        startOta(/*asAccessPoint=*/true);
    }
}

void loop()
{
    // ... normal gateway/application work goes here ...

    if (ota && !ota->handle())
    {
        Serial.println("[OTA] Window closed, shutting OTA down.");
        ota->end();
        delete ota;
        ota = nullptr;
    }
}
