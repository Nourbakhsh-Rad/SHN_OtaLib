// Example: instead of joining an existing WiFi network, the device creates
// its own Access Point so a technician can connect a laptop/phone directly
// to it in the field - useful when there's no site WiFi available yet.
//
// NOTE on the PIN printed below: the default pages include a challenge-auth
// script that regenerates the PIN from a fresh random salt every time the
// root page ("/") is loaded (see the README's "built-in pages and their
// endpoints" section). That means the PIN printed once at startup can go
// stale the moment someone opens the root page in a browser. onPasswordChanged
// below reprints it every time that happens, so Serial always shows the
// PIN that's actually valid right now. Set cfg.challengeAuth = false instead
// if you'd rather have one fixed PIN for the whole session (the root page's
// "Upload Service" link still works either way).
//
// cfg.pepper is left empty here for a quick local test. For anything beyond
// that - and especially before this repo is public - see the README's
// "Security model" section and set cfg.pepper = OTA_SECRET_PEPPER from your
// own (gitignored) OtaSecrets.h.

#include <SHN_OtaLib.h>

OtaManager* ota;

void setup()
{
    Serial.begin(115200);

    OtaManager::Config cfg;
    cfg.hardwareId   = "IP-BOX";
    cfg.swVersion    = "1.2.0";

    cfg.asAccessPoint = true;
    cfg.wifiSsid      = "IPBOX-SETUP";     // AP name the technician connects to
    cfg.wifiPassword  = "12345678";        // min 8 chars, or "" for an open AP

    cfg.activeMinutes = 20;                // auto-close the AP+OTA window

    cfg.onPasswordChanged = [](const String& newPassword) {
        Serial.print("Update PIN changed, now: ");
        Serial.println(newPassword);
    };

    ota = new OtaManager(cfg);

    if (ota->begin())
    {
        Serial.println("Access Point started.");
        Serial.print("Connect to WiFi: ");    Serial.println(cfg.wifiSsid);
        Serial.print("Then open: ");          Serial.println(ota->updateUrl());
        Serial.print("Update PIN: ");         Serial.println(ota->updatePassword());
    }
    else
    {
        Serial.println("Failed to start Access Point.");
    }
}

void loop()
{
    if (ota && !ota->handle())
    {
        ota->end();
        delete ota;
        ota = nullptr;
        Serial.println("Setup window closed - AP is now off.");
    }
}
