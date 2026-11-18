//-----------------------------------------------------------------------------
// IPageProvider.h
// Abstraction over "where do the OTA web pages come from". Lets HttpOtaProvider
// stay agnostic between embedded PROGMEM pages and filesystem-served pages.
//-----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include "OtaPlatform.h"

struct OtaPageContext
{
    String deviceId;
    String swVersion;
    String hardwareId;
    String updateUrl;   // e.g. "192.168.1.50:8080/update"
};

class IPageProvider
{
public:
    virtual ~IPageProvider() = default;

    // Serve the root ("/") status/info page.
    virtual void serveRoot(OtaWebServer& server, const OtaPageContext& ctx) = 0;

    // Serve the firmware-upload page (behind auth, handled by the caller).
    virtual void serveUploadPage(OtaWebServer& server, const OtaPageContext& ctx) = 0;

    // Serve a 404 page.
    virtual void serveNotFound(OtaWebServer& server) = 0;
};
