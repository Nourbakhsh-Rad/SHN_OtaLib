# Changelog

## 2.0.0

Initial public release: a modular, class-based rewrite of the legacy
`SHN_OtaUtil` (v1.4.8) for ESP32 and ESP8266.

- HTTP firmware + filesystem update server (`HttpOtaProvider`), optional FTP
  firmware upload (`FtpFirmwareUpdater`, opt-in via `OTA_ENABLE_FTP`),
  `OtaManager` facade, `WifiConnector`, swappable page providers.
- PIN scheme: `Config::pepper` now actually influences the PIN (it was
  previously appended past the part of the input the checksum reads, so it
  had no effect). With a pepper set the PIN is derived from an FNV-1a hash of
  identity + salt + pepper; with an empty pepper the legacy checksum is
  unchanged. `tools/OTA_pass.html` implements the same algorithm.
- `OtaManager::Config::wifiConnectTimeoutSec` defaults to 15 s (was 0, which
  made `begin()` fail for station mode unless WiFi was already up).
- Auth lockout on `/update`, wrap-safe OTA window timer, `FsPageProvider`
  answers custom 404 pages with status 404.
