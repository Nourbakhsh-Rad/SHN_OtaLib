//-----------------------------------------------------------------------------
// FsPageProvider.h
// Serves pages from a filesystem (LittleFS/SPIFFS/FFat - whichever OTA_FS
// resolves to in OtaPlatform.h) so a project can ship custom-branded pages
// without recompiling the library. Falls back to a plain-text 404 if a file
// is missing, rather than the old goto-based "load /index_notfound.html or
// die trying" chain.
//-----------------------------------------------------------------------------
#pragma once

#include "IPageProvider.h"
#include <FS.h>
#include <utility>

class FsPageProvider : public IPageProvider
{
public:
    FsPageProvider(String rootPath   = "/index_root.html",
                    String uploadPath = "/index_upload.html",
                    String notFoundPath = "/index_notfound.html")
        : _rootPath(std::move(rootPath))
        , _uploadPath(std::move(uploadPath))
        , _notFoundPath(std::move(notFoundPath))
    {}

    void serveRoot(OtaWebServer& server, const OtaPageContext& ctx) override
    {
        serveTemplated(server, _rootPath, ctx);
    }

    void serveUploadPage(OtaWebServer& server, const OtaPageContext& ctx) override
    {
        serveTemplated(server, _uploadPath, ctx);
    }

    void serveNotFound(OtaWebServer& server) override
    {
        if (OTA_FS.exists(_notFoundPath))
        {
            File f = OTA_FS.open(_notFoundPath, "r");
            String page = f.readString();
            f.close();
            server.send(404, "text/html", page);   // streamFile() would answer 200
        }
        else
        {
            server.send(404, "text/plain", F("404: not found"));
        }
    }

private:
    String _rootPath;
    String _uploadPath;
    String _notFoundPath;

    void serveTemplated(OtaWebServer& server, const String& path, const OtaPageContext& ctx)
    {
        if (!OTA_FS.exists(path))
        {
            serveNotFound(server);
            return;
        }

        File f = OTA_FS.open(path, "r");
        String page = f.readString();
        f.close();

        page.replace("_hw_", ctx.hardwareId);
        page.replace("_id_", ctx.deviceId);
        page.replace("_sw_", ctx.swVersion);
        page.replace("_url_", ctx.updateUrl);

        server.send(200, "text/html", page);
    }
};
