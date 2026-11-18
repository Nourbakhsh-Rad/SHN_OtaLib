//-----------------------------------------------------------------------------
// HttpOtaProvider.h
// Wraps WebServer/ESP8266WebServer behind a small, non-macro-driven API.
// Replaces the ArduinoOTA / HTTP_UPLOADER_FS / AsyncElegantOTA branches of
// the legacy library with a single, focused implementation (AsyncElegantOTA
// is unmaintained upstream; ArduinoOTA is dropped per project decision to
// standardize on one method).
//
// The /update upload handler is implemented directly against the Update API
// (rather than wrapping HTTPUpdateServer) for two reasons specific to this
// project's pages:
//   1. The upload form can flash either firmware or a filesystem image
//      (its "Firmware" / "FileSystem" switch), which HTTPUpdateServer alone
//      doesn't support.
//   2. The password can change at runtime (see /PassHash.php in the
//      challenge-auth block below), and HTTPUpdateServer bakes the password
//      it was given into itself at setup() time - it can't be updated later.
//-----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <utility>
#include "OtaPlatform.h"
#include "OtaCredentials.h"
#include "IPageProvider.h"
#include "EmbeddedPageProvider.h"

class HttpOtaProvider
{
public:
    struct Config
    {
        uint16_t port          = 8080;
        String   user          = "admin";
        String   password;                 // empty -> auto-generate on begin()
        String   deviceId;                 // e.g. from OtaCredentials::deviceId()
        String   hardwareId    = "DEVICE";
        String   swVersion     = "1.0.0";
        uint32_t activeMinutes = 5;         // 0 = stay open indefinitely
        IPageProvider* pageProvider = nullptr; // nullptr -> EmbeddedPageProvider

        // Enables the /PassHash.php + /start.php endpoints that the root
        // page's script calls after reading identity: it generates a random
        // on-screen salt and asks the device to fold it into the PIN
        // (OtaCredentials::generatePin's `salt` argument) - the "read this
        // code to support over the phone" workflow. Each page load produces
        // a NEW effective password when this is on - see onPasswordChanged
        // below if you want to know what it currently is.
        //
        // /getip.php and /identity.php are unaffected by this flag - they
        // carry no secret, and the root page needs them just to show the
        // device identity and set the "Upload Service" link's target.
        bool challengeAuth = true;

        // Fired every time the effective password changes - i.e. whenever
        // /PassHash.php regenerates it from a fresh salt. Handy for logging
        // the current, actually-valid PIN to Serial during testing, since
        // the one auto-generated at begin() stops being valid the moment
        // someone loads the root page (if challengeAuth is on).
        using PasswordChangedCallback = void (*)(const String& newPassword);
        PasswordChangedCallback onPasswordChanged = nullptr;

        // The actual secret. generatePin()'s checksum algorithm is public
        // (it's in this source file); pepper is what needs real entropy -
        // set it to a long, random, per-deployment string and keep it OUT
        // of version control (see tools/OtaSecrets.example.h in this repo).
        // Leaving it empty means the PIN's only "secrecy" is whether
        // someone has read this file - assume they have.
        String pepper;
        OtaCredentials::PinOptions pinOptions;   // only change if you need a
                                                  // different PIN length/charset;
                                                  // doesn't add real security on its own

        // Failed-auth throttling on /update: after this many consecutive
        // failures, further attempts are rejected outright (without even
        // checking credentials) for lockoutMs, doubling on each repeat
        // offense up to lockoutMaxMs. Resets to the base duration on any
        // successful auth. This is what actually makes brute-forcing the
        // PIN (or any small pinOptions combination) impractical over the
        // network - the PIN math alone does not.
        uint8_t  lockoutThreshold = 5;
        uint32_t lockoutMs        = 30000;      // 30s initial lockout
        uint32_t lockoutMaxMs     = 1800000;    // cap at 30 minutes
    };

    explicit HttpOtaProvider(Config config)
        : _config(std::move(config))
        , _server(_config.port)
    {
        if (!_config.pageProvider)
        {
            static EmbeddedPageProvider defaultProvider;
            _config.pageProvider = &defaultProvider;
        }
    }

    bool begin(IPAddress ip)
    {
        _ip = ip;

        if (_config.password.isEmpty())
        {
            _config.password = OtaCredentials::generatePin(
                _config.deviceId, _config.swVersion, _config.hardwareId,
                "", _config.pepper, _config.pinOptions);
        }

        setupRoutes();
        _server.begin();

        _hasDeadline = _config.activeMinutes != 0;
        _deadlineMs  = millis() + (uint32_t)_config.activeMinutes * 60000UL;
        _active = true;
        return true;
    }

    // Call every loop(). Returns false once the active window has expired
    // (caller decides what to do next - e.g. stop WiFi, go back to normal
    // operation). Unlike the legacy library, expiry never blocks or resets
    // the device by itself. A successful firmware/filesystem flash still
    // ends with a brief (~1s) blocking delay + ESP.restart(), same as any
    // standard web-OTA handler - that's needed to let the HTTP response
    // flush before reboot, not the old unbounded freeze.
    bool handle()
    {
        if (!_active) return false;

        if (_hasDeadline && (int32_t)(millis() - _deadlineMs) >= 0)
        {
            end();
            return false;
        }

        _server.handleClient();
        return true;
    }

    void end()
    {
        _server.close();
        _active = false;
    }

    bool isActive() const { return _active; }
    String currentPassword() const { return _config.password; }
    String updateUrl() const
    {
        return _ip.toString() + ":" + String(_config.port) + "/update";
    }

private:
    Config        _config;
    OtaWebServer  _server;
    IPAddress     _ip;
    uint32_t      _deadlineMs = 0;
    bool          _hasDeadline = false;
    bool          _active = false;
    bool          _uploadAuthOk = false;
    bool          _uploadHadError = false;
    uint8_t       _failedAttempts = 0;
    uint32_t      _lockoutUntilMs = 0;
    uint32_t      _currentLockoutMs = 0;   // 0 until first lockout, then = config.lockoutMs and doubles

    void setupRoutes()
    {
        _server.onNotFound([this]() {
            _config.pageProvider->serveNotFound(_server);
        });

        _server.on("/", HTTP_GET, [this]() {
            _config.pageProvider->serveRoot(_server, pageContext());
        });

        _server.on("/update", HTTP_GET, [this]() {
            if (isLockedOut())
            {
                _server.send(429, "text/plain", lockoutMessage());
                return;
            }
            if (!_server.authenticate(_config.user.c_str(), _config.password.c_str()))
            {
                recordAuthFailure();
                return _server.requestAuthentication();
            }
            recordAuthSuccess();
            _config.pageProvider->serveUploadPage(_server, pageContext());
        });

        _server.on("/update", HTTP_POST,
            [this]() { onUpdateFinished(); },
            [this]() { onUpdateChunk(); });

        _server.on("/identity", HTTP_GET, [this]() {
            String resp = "HW:" + _config.hardwareId +
                          " ID:" + _config.deviceId +
                          " SW:" + _config.swVersion;
            _server.send(200, "text/plain", resp);
        });

        // Always available: no secret in either response, and the root
        // page needs them regardless of whether the PIN-regeneration
        // challenge flow (below) is enabled.
        _server.on("/getip.php", HTTP_POST, [this]() {
            String resp = "//" + updateUrl();
            _server.send(200, "text/plain", resp);
        });

        _server.on("/identity.php", HTTP_GET, [this]() { identityPhpHandler(); });
        _server.on("/identity.php", HTTP_POST, [this]() { identityPhpHandler(); });

        if (_config.challengeAuth)
        {
            _server.on("/PassHash.php", HTTP_POST, [this]() {
                // Client sends a random on-screen salt ("Phash"); folding it
                // into the PIN means the effective password changes every
                // time this page is loaded, and support can compute the
                // matching PIN remotely from the same salt + device facts.
                String salt = _server.hasArg("Phash") ? _server.arg("Phash") : String();

                _config.password = OtaCredentials::generatePin(
                    _config.deviceId, _config.swVersion, _config.hardwareId,
                    salt, _config.pepper, _config.pinOptions);

                if (_config.onPasswordChanged) _config.onPasswordChanged(_config.password);

                _server.send(200, "text/plain", "<br>Pass. Hash: " + salt);
            });

            // No-op on purpose: the legacy library froze the whole device
            // here while waiting for the upload. Our /update handler below
            // does the actual flashing synchronously inside its own POST
            // handler, so nothing extra needs to happen here - this
            // endpoint only exists so the existing page JS (which calls it
            // right before the real upload) has something to hit.
            _server.on("/start.php", HTTP_POST, [this]() {
                _server.send(200, "text/plain", "");
            });
        }
    }

    void identityPhpHandler()
    {
        String resp = "Device HW: " + _config.hardwareId +
                      "<br>Device ID: " + _config.deviceId +
                      "<br>Software Ver: " + _config.swVersion +
                      "<span id=\"hsh\"></span>";
        _server.send(200, "text/plain", resp);
    }

    // Streams each chunk of the uploaded file straight into the Update API.
    // Matches the upload page's <input name="firmware"> / name="filesystem">
    // switch to decide which partition gets written.
    void onUpdateChunk()
    {
        HTTPUpload& upload = _server.upload();

        if (upload.status == UPLOAD_FILE_START)
        {
            _uploadHadError = false;

            if (isLockedOut())
            {
                _uploadAuthOk = false;
                return;
            }

            _uploadAuthOk = _server.authenticate(_config.user.c_str(), _config.password.c_str());
            if (!_uploadAuthOk)
            {
                recordAuthFailure();
                return;
            }
            recordAuthSuccess();

            int cmd = (upload.name == "filesystem") ? OTA_UPDATE_CMD_FS : OTA_UPDATE_CMD_FLASH;

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
            {
                Update.printError(Serial);
                _uploadHadError = true;
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (!_uploadAuthOk || _uploadHadError) return;

            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
                _uploadHadError = true;
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (!_uploadAuthOk || _uploadHadError) return;

            if (!Update.end(true))
            {
                Update.printError(Serial);
                _uploadHadError = true;
            }
        }
        else if (upload.status == UPLOAD_FILE_ABORTED)
        {
            Update.end(false);
            _uploadHadError = true;
        }
    }

    void onUpdateFinished()
    {
        // Take and clear the per-upload flags so a later POST that carries
        // no file part can never inherit this upload's auth state.
        const bool authOk  = _uploadAuthOk;
        const bool hadErr  = _uploadHadError;
        _uploadAuthOk   = false;
        _uploadHadError = false;

        if (!authOk)
        {
            return _server.requestAuthentication();
        }

        _server.sendHeader("Connection", "close");

        if (hadErr || Update.hasError())
        {
            // Send the Update API's own error string back so this is
            // diagnosable from the browser alone, without needing a serial
            // monitor. NOTE: a message here (e.g. write/space/MD5 errors)
            // means the write itself failed and nothing changed. If instead
            // the panel shows "Update Success! Rebooting..." but the device
            // then fails to boot with a decryption error on Serial, that is
            // a DIFFERENT problem - it means the write succeeded but the
            // bootloader rejected the new image (commonly flash encryption
            // being enabled without the image being written in the format
            // it expects) - Update.hasError() has no visibility into that,
            // since it happens after this HTTP response is already sent.
            _server.send(200, "text/plain",
                         "Update failed: " + OtaPlatform_updateErrorString());
            return;
        }

        _server.send(200, "text/plain", "Update Success! Rebooting...");
        delay(1000);   // let the response actually reach the browser first
        ESP.restart();
    }

    OtaPageContext pageContext() const
    {
        return OtaPageContext{
            _config.deviceId,
            _config.swVersion,
            _config.hardwareId,
            updateUrl()
        };
    }

    bool isLockedOut() const
    {
        return _lockoutUntilMs != 0 && (long)(millis() - _lockoutUntilMs) < 0;
    }

    void recordAuthFailure()
    {
        if (++_failedAttempts < _config.lockoutThreshold) return;

        _currentLockoutMs = _currentLockoutMs == 0
                                ? _config.lockoutMs
                                : min(_currentLockoutMs * 2, _config.lockoutMaxMs);
        _lockoutUntilMs = millis() + _currentLockoutMs;
        _failedAttempts = 0;
    }

    void recordAuthSuccess()
    {
        _failedAttempts   = 0;
        _currentLockoutMs = 0;
        _lockoutUntilMs   = 0;
    }

    String lockoutMessage() const
    {
        uint32_t remainingSec = (_lockoutUntilMs - millis()) / 1000;
        return "Too many failed attempts. Try again in " + String(remainingSec) + "s.";
    }
};
