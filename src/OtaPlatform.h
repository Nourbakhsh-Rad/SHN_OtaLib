//-----------------------------------------------------------------------------
// OtaPlatform.h
// Isolates every ESP8266 / ESP32 API difference in ONE place so the rest of
// the library never needs to #ifdef on the platform again.
//-----------------------------------------------------------------------------
#pragma once

#if defined(ESP32)
    #include <WiFi.h>
    #include <WiFiClient.h>
    #include <WebServer.h>
    #include <ESPmDNS.h>
    #include <Update.h>

    using OtaWebServer   = WebServer;

    #define OTA_UPDATE_CMD_FLASH U_FLASH
    #define OTA_UPDATE_CMD_FS    U_SPIFFS

    inline String OtaPlatform_chipId()
    {
        String id = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
        id += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
        id.toUpperCase();
        return id;
    }

    inline String OtaPlatform_updateErrorString()
    {
        return String(Update.errorString());
    }

#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266WebServer.h>
    #include <ESP8266mDNS.h>
    #include <Updater.h>

    using OtaWebServer   = ESP8266WebServer;

    #define OTA_UPDATE_CMD_FLASH U_FLASH
    #define OTA_UPDATE_CMD_FS    U_FS

    inline String OtaPlatform_chipId()
    {
        String id = String(ESP.getChipId(), HEX);
        id.toUpperCase();
        return id;
    }

    inline String OtaPlatform_updateErrorString()
    {
        return Update.getErrorString();
    }

#else
    #error "SHN_OtaLib: only ESP32 and ESP8266 are supported."
#endif

// Filesystem back-end selection is independent of the WiFi/CPU platform,
// so it stays a compile-time choice made by the sketch (or build flags),
// not something guessed from the target chip.
#if !defined(OTA_FS_MODE_LITTLEFS) && !defined(OTA_FS_MODE_SPIFFS) && !defined(OTA_FS_MODE_FATFS)
    #define OTA_FS_MODE_LITTLEFS 1   // default: LittleFS
#endif

#if defined(OTA_FS_MODE_LITTLEFS)
    #include <LittleFS.h>
    #define OTA_FS LittleFS
#elif defined(OTA_FS_MODE_FATFS)
    #if defined(ESP8266)
        #error "SHN_OtaLib: FATFS is only available on ESP32."
    #endif
    #include <FFat.h>
    #define OTA_FS FFat
#elif defined(OTA_FS_MODE_SPIFFS)
    #include <FS.h>
    #if defined(ESP32)
        #include <SPIFFS.h>
    #endif
    #define OTA_FS SPIFFS
#endif
