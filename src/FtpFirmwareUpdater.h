//-----------------------------------------------------------------------------
// FtpFirmwareUpdater.h
// Optional module: accepts "firmware.bin" over FTP and flashes it via the
// Update API. Fully independent of HttpOtaProvider - a sketch that doesn't
// need FTP simply never includes/instantiates this class, so it costs
// nothing when unused (unlike the legacy library, where FTP support was
// wired into the same #if chain as everything else).
//
// NOTE: SimpleFTPServer's callback API takes plain function pointers, not
// member functions, so this class keeps one static self-pointer internally
// to dispatch into instance methods. This only supports a single active
// FtpFirmwareUpdater at a time, which matches real usage (one device, one
// FTP server).
//-----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include "OtaPlatform.h"   // pulls in Update.h / Updater.h for the right chip
#include <SimpleFTPServer.h>

class FtpFirmwareUpdater
{
public:
    using DoneCallback = void (*)(bool success);

    ~FtpFirmwareUpdater()
    {
        if (_self == this) _self = nullptr;   // never leave the trampolines dangling
    }

    bool begin(const String& user, const String& password, const String& firmwareFileName = "/firmware.bin")
    {
        _self = this;
        _firmwareFileName = firmwareFileName;

        _server.setCallback(&FtpFirmwareUpdater::ftpEventTrampoline);
        _server.setTransferCallback(&FtpFirmwareUpdater::transferEventTrampoline);

        return password.length()
                   ? _server.begin(user.c_str(), password.c_str())
                   : _server.begin(user.c_str(), "");
    }

    void handle() { _server.handleFTP(); }

    void onUpdateComplete(DoneCallback cb) { _onDone = cb; }

private:
    FtpServer     _server;
    String        _firmwareFileName;
    volatile bool _uploaded = false;
    DoneCallback  _onDone = nullptr;

    static FtpFirmwareUpdater* _self;

    static void ftpEventTrampoline(FtpOperation op, unsigned long freeSpace, unsigned long totalSpace)
    {
        if (!_self) return;
        if (op == FTP_FREE_SPACE_CHANGE && _self->_uploaded)
        {
            _self->_uploaded = false;
            _self->flashFirmware();
        }
    }

    static void transferEventTrampoline(FtpTransferOperation op, const char* name, unsigned long)
    {
        if (!_self) return;
        if (op == FTP_TRANSFER_STOP && String(name).indexOf(_self->_firmwareFileName) >= 0)
        {
            _self->_uploaded = true;
        }
    }

    void flashFirmware()
    {
        File firmware = OTA_FS.open(_firmwareFileName, "r");
        bool ok = false;

        if (firmware)
        {
            const size_t size = firmware.size();
            if (Update.begin(size, OTA_UPDATE_CMD_FLASH))
            {
                const bool wrote = (Update.writeStream(firmware) == size);
                ok = Update.end() && wrote;
            }
            firmware.close();

            if (ok)
            {
                String renamed = _firmwareFileName;
                renamed.replace(".bin", ".bak");
                OTA_FS.rename(_firmwareFileName, renamed.c_str());
            }
        }

        if (_onDone) _onDone(ok);
    }
};

inline FtpFirmwareUpdater* FtpFirmwareUpdater::_self = nullptr;
