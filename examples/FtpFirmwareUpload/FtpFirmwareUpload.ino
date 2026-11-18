// Example: enable the optional FTP firmware-upload path alongside the
// normal HTTP OTA server. FtpFirmwareUpdater is only compiled in when
// OTA_ENABLE_FTP is defined BEFORE including SHN_OtaLib.h - so sketches
// that don't need FTP never pull in SimpleFTPServer at all.

#define OTA_ENABLE_FTP
#include <SHN_OtaLib.h>

OtaManager* ota;

void setup()
{
    Serial.begin(115200);

    OtaManager::Config cfg;
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.2.0";
    cfg.wifiSsid      = "your-ssid";
    cfg.wifiPassword  = "your-password";
    cfg.wifiConnectTimeoutSec = 15;
    cfg.activeMinutes = 0;        // 0 = HTTP OTA window stays open indefinitely

    cfg.enableFtp   = true;
    cfg.ftpUser     = "admin";
    cfg.ftpPassword = "change-me";   // FTP has no PIN auto-generation; set it explicitly
    cfg.ftpDoneCallback = [](bool success) {
        Serial.println(success ? "FTP firmware flash OK - rebooting is up to you."
                                : "FTP firmware flash FAILED.");
    };

    ota = new OtaManager(cfg);

    if (ota->begin())
    {
        Serial.print("HTTP OTA ready at: ");
        Serial.println(ota->updateUrl());
        Serial.print("HTTP update PIN: ");
        Serial.println(ota->updatePassword());
        Serial.println("FTP firmware upload: put firmware.bin on the device "
                        "root over FTP to trigger a flash.");
    }
    else
    {
        Serial.println("WiFi connection failed.");
    }
}

void loop()
{
    if (ota) ota->handle();   // with activeMinutes == 0 this never returns false
}
