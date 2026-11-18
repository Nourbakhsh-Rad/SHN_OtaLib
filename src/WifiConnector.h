//-----------------------------------------------------------------------------
// WifiConnector.h
// Small, standalone WiFi bring-up helper. Deliberately has no OTA-specific
// knowledge so it can be reused anywhere else in the firmware too.
//-----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include "OtaPlatform.h"

class WifiConnector
{
public:
    enum class Mode { Station, AccessPoint };

    // connectTimeoutSec == 0 means "don't block, return immediately".
    bool begin(const String& ssid,
               const String& password,
               const String& hostname,
               Mode mode = Mode::Station,
               uint32_t connectTimeoutSec = 0)
    {
        _mode = mode;

        if (mode == Mode::AccessPoint)
        {
            WiFi.mode(WIFI_AP);

            _ok = password.length()
                      ? WiFi.softAP(ssid.c_str(), password.c_str())
                      : WiFi.softAP(ssid.c_str());

            delay(200);

#if defined(ESP32)
            if (_ok && hostname.length())
            {
                WiFi.softAPsetHostname(hostname.c_str());
            }
#endif
            if (_ok)
            {
                _ip = WiFi.softAPIP();
            }
            return _ok;
        }

        //--- Station mode ---
        WiFi.mode(WIFI_STA);

        if (hostname.length())
        {
            WiFi.setHostname(hostname.c_str());
        }

        WiFi.begin(ssid.c_str(), password.c_str());

        if (connectTimeoutSec > 0)
        {
            const uint32_t deadline = millis() + connectTimeoutSec * 1000UL;
            while (WiFi.status() != WL_CONNECTED && millis() < deadline)
            {
                delay(250);
            }
        }

        _ok = (WiFi.status() == WL_CONNECTED);
        if (_ok)
        {
            _ip = WiFi.localIP();
        }
        return _ok;
    }

    bool isConnected() const
    {
        if (_mode == Mode::AccessPoint) return _ok;
        return WiFi.status() == WL_CONNECTED;
    }

    IPAddress ip() const { return _ip; }

    void end(bool turnOffRadio = true)
    {
        if (turnOffRadio)
        {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
        }
        _ok = false;
    }

private:
    Mode      _mode = Mode::Station;
    bool      _ok   = false;
    IPAddress _ip;
};
