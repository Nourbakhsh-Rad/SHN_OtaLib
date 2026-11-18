//-----------------------------------------------------------------------------
// OtaManager.h
// Top-level facade: WifiConnector + HttpOtaProvider + optional
// FtpFirmwareUpdater, wired together. This is the one class most sketches
// will actually use; the pieces above stay available individually for
// projects that want to assemble things differently.
//-----------------------------------------------------------------------------
#pragma once

#include <memory>
#include <utility>
#include "WifiConnector.h"
#include "HttpOtaProvider.h"
#include "OtaCredentials.h"

#ifdef OTA_ENABLE_FTP
    #include "FtpFirmwareUpdater.h"
#endif

class OtaManager
{
public:
    // Fired once the OTA service is actually reachable, whichever begin()
    // overload got there - self-managed WiFi coming up, or an
    // already-connected network handed in directly.
    using NetworkUpCallback = void (*)(IPAddress ip);

    // Fired when a WiFi connection that OtaManager itself brought up (via
    // the no-argument begin()) is later detected as dropped, during
    // handle(). Never fired for a network passed in via begin(ip) - that
    // network isn't OtaManager's to monitor.
    using NetworkDownCallback = void (*)();

    struct Config
    {
        // Identity
        String deviceId;     // empty -> auto (OtaCredentials::deviceId())
        String hardwareId    = "DEVICE";
        String swVersion     = "1.0.0";

        // WiFi (only used by the no-argument begin())
        String   wifiSsid;
        String   wifiPassword;
        bool     asAccessPoint = false;
        uint32_t wifiConnectTimeoutSec = 15;   // begin() returns false if STA is not up by then

        // Network state callbacks
        NetworkUpCallback   onNetworkUp   = nullptr;
        NetworkDownCallback onNetworkDown = nullptr;

        // HTTP OTA
        uint16_t httpPort      = 8080;
        String   httpUser      = "admin";
        String   httpPassword;           // empty -> auto-generated PIN
        uint32_t activeMinutes = 5;
        IPageProvider* pageProvider = nullptr;
        bool     challengeAuth = true;   // see HttpOtaProvider::Config::challengeAuth
        HttpOtaProvider::Config::PasswordChangedCallback onPasswordChanged = nullptr;

        // See HttpOtaProvider::Config for what these actually protect
        // against - pepper is the real secret; pinOptions and the lockout
        // fields are secondary.
        String pepper;
        OtaCredentials::PinOptions pinOptions;
        uint8_t  lockoutThreshold = 5;
        uint32_t lockoutMs        = 30000;
        uint32_t lockoutMaxMs     = 1800000;

#ifdef OTA_ENABLE_FTP
        bool   enableFtp    = false;
        String ftpUser      = "admin";
        String ftpPassword;
        FtpFirmwareUpdater::DoneCallback ftpDoneCallback = nullptr;
#endif
    };

    explicit OtaManager(Config config) : _config(std::move(config))
    {
        if (_config.deviceId.isEmpty())
        {
            _config.deviceId = OtaCredentials::deviceId();
        }
    }

    // Connects WiFi itself (STA or AP, per Config), then starts the HTTP OTA
    // server (and FTP, if enabled). Returns false if WiFi never came up -
    // onNetworkUp is only called on success.
    bool begin()
    {
        String hostname = _config.hardwareId + "-" + _config.deviceId;

        bool wifiOk = _wifi.begin(
            _config.wifiSsid,
            _config.wifiPassword,
            hostname,
            _config.asAccessPoint ? WifiConnector::Mode::AccessPoint
                                   : WifiConnector::Mode::Station,
            _config.wifiConnectTimeoutSec);

        if (!wifiOk) return false;

        _networkManagedHere = true;
        _wasConnected = true;
        startServices(_wifi.ip());
        return true;
    }

    // Overload for when the network is already up and OtaManager shouldn't
    // touch it - e.g. the board only ever talks over Ethernet (ETH.begin()
    // already called, pass ETH.localIP()), or the sketch connected WiFi
    // itself (pass WiFi.localIP()). OtaManager just starts the HTTP OTA
    // server (+ FTP) bound to this address; it never brings this network up
    // or tears it down, and never monitors it for drops (isConnected() just
    // reports true, onNetworkDown is never called for it).
    bool begin(IPAddress ip)
    {
        _networkManagedHere = false;
        startServices(ip);
        if (_config.onNetworkUp) _config.onNetworkUp(ip);
        return true;
    }

    // Call from loop(). Returns false once the OTA window has closed, so the
    // caller can decide to tear WiFi down and resume normal operation - the
    // library never forces a device reset or an infinite blocking loop.
    // Also where a self-managed WiFi disconnect gets noticed and reported
    // via onNetworkDown (edge-triggered: fires once per drop, not every
    // loop while still down).
    bool handle()
    {
        if (_networkManagedHere)
        {
            bool connectedNow = _wifi.isConnected();
            if (_wasConnected && !connectedNow && _config.onNetworkDown)
            {
                _config.onNetworkDown();
            }
            _wasConnected = connectedNow;
        }

        bool stillActive = _http && _http->handle();

#ifdef OTA_ENABLE_FTP
        if (_ftp) _ftp->handle();
#endif
        return stillActive;
    }

    bool isConnected() const { return _networkManagedHere ? _wifi.isConnected() : true; }
    IPAddress ip() const { return _networkManagedHere ? _wifi.ip() : _externalIp; }
    String updatePassword() const { return _http ? _http->currentPassword() : String(); }
    String updateUrl() const { return _http ? _http->updateUrl() : String(); }

    // Stops the HTTP OTA server (and FTP). Only tears WiFi down if
    // OtaManager was the one that brought it up via the no-argument begin()
    // - a network handed in via begin(ip) (Ethernet, or WiFi managed by the
    // sketch itself) is left exactly as it was.
    void end()
    {
        if (_http) _http->end();
#ifdef OTA_ENABLE_FTP
        _ftp.reset();
#endif
        if (_networkManagedHere) _wifi.end();
    }

private:
    Config _config;
    WifiConnector _wifi;
    IPAddress _externalIp;
    bool _networkManagedHere = true;
    bool _wasConnected = false;
    std::unique_ptr<HttpOtaProvider> _http;
#ifdef OTA_ENABLE_FTP
    std::unique_ptr<FtpFirmwareUpdater> _ftp;
#endif

    void startServices(IPAddress ip)
    {
        _externalIp = ip;

        HttpOtaProvider::Config httpCfg;
        httpCfg.port          = _config.httpPort;
        httpCfg.user          = _config.httpUser;
        httpCfg.password      = _config.httpPassword;
        httpCfg.deviceId      = _config.deviceId;
        httpCfg.hardwareId    = _config.hardwareId;
        httpCfg.swVersion     = _config.swVersion;
        httpCfg.activeMinutes = _config.activeMinutes;
        httpCfg.pageProvider  = _config.pageProvider;
        httpCfg.challengeAuth = _config.challengeAuth;
        httpCfg.onPasswordChanged = _config.onPasswordChanged;
        httpCfg.pepper           = _config.pepper;
        httpCfg.pinOptions       = _config.pinOptions;
        httpCfg.lockoutThreshold = _config.lockoutThreshold;
        httpCfg.lockoutMs        = _config.lockoutMs;
        httpCfg.lockoutMaxMs     = _config.lockoutMaxMs;

        _http = std::unique_ptr<HttpOtaProvider>(new HttpOtaProvider(httpCfg));
        _http->begin(ip);

        if (_networkManagedHere && _config.onNetworkUp) _config.onNetworkUp(ip);

#ifdef OTA_ENABLE_FTP
        if (_config.enableFtp)
        {
            _ftp = std::unique_ptr<FtpFirmwareUpdater>(new FtpFirmwareUpdater());
            if (_config.ftpDoneCallback) _ftp->onUpdateComplete(_config.ftpDoneCallback);
            _ftp->begin(_config.ftpUser, _config.ftpPassword);
        }
#endif
    }
};
