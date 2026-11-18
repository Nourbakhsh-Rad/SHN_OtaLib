# SHN_OtaLib

![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-00979D?logo=arduino)](https://www.arduino.cc/)
[![ESP32](https://img.shields.io/badge/ESP32-Supported-E7352C)](https://www.espressif.com/en/products/socs/esp32)
[![ESP8266](https://img.shields.io/badge/ESP8266-Supported-000000)](https://www.espressif.com/en/products/socs/esp8266)

Modular HTTP-based OTA + optional FTP firmware update library for **`ESP32/ESP8266`**

**Version:** 2.0.0  
**License:** MIT

---

A modular, class-based OTA (over-the-air) update library for **ESP32** and
**ESP8266**, built as an independent rewrite of the legacy `SHN_OtaUtil`
(v1.4.8). It serves a firmware/filesystem update page over HTTP, optionally
accepts firmware over FTP, and works whether the board manages its own WiFi,
already has WiFi connected, or only ever talks over Ethernet.

- ✅ ESP32 and ESP8266
- ✅ HTTP firmware **and** filesystem (LittleFS/SPIFFS) update, from one page
- ✅ Optional FTP firmware upload, fully opt-in (zero cost if unused)
- ✅ Works over WiFi (STA or AP, self-managed) **or** any pre-existing
  network (Ethernet, or WiFi connected by your own sketch)
- ✅ Time-boxed OTA windows — no infinite blocking, no forced device freeze
- ✅ Swappable page source: built-in flash pages or your own from a filesystem
- ✅ Network up/down callbacks
- ✅ Pages are mobile-friendly (viewport meta tag, fluid layout, no fixed
  widths that force horizontal scrolling on phones)

## Installation

**Arduino IDE**
1. Download/clone this repository.
2. Copy the `SHN_OtaLib` folder into your Arduino `libraries/` folder
   (`~/Documents/Arduino/libraries/` on most systems).
3. Restart the Arduino IDE. `File > Examples > SHN_OtaLib` will show the
   example sketches.

**PlatformIO**

```ini
lib_deps =
    https://github.com/Nourbakhsh-Rad/SHN_OtaLib.git
```

If you enable the optional FTP module (see below), also add
[SimpleFTPServer](https://github.com/xreef/SimpleFTPServer) as a dependency —
it's declared in `library.properties` but PlatformIO/Arduino Library Manager
installs may still need it added explicitly depending on your setup.

**Before your first build**, set up the real secret (see "Security model"
below for why this matters):

```bash
cp tools/OtaSecrets.example.h <your-sketch-folder>/OtaSecrets.h
# edit OtaSecrets.h, replace the placeholder with a long random string
```

Put `OtaSecrets.h` in **your own sketch/project**, not inside the library
folder (a library update would delete it). Then in your sketch:

```cpp
#include "OtaSecrets.h"   // OTA_SECRET_PEPPER
...
cfg.pepper = OTA_SECRET_PEPPER;
```

A file named `OtaSecrets.h` is in this repo's `.gitignore`; add the same
line to your own project's `.gitignore`. Copy `tools/OTA_pass.html`, fill in
the same value in its `Pepper` field, and keep that filled-in copy out of git
too (name it `OTA_pass.local.html`, already ignored here).

## Quick start

```cpp
#include <SHN_OtaLib.h>

OtaManager* ota;

void setup() {
    Serial.begin(115200);

    OtaManager::Config cfg;
    cfg.hardwareId    = "IP-BOX";
    cfg.swVersion     = "1.0.0";
    cfg.wifiSsid      = "your-ssid";
    cfg.wifiPassword  = "your-password";
    cfg.activeMinutes = 10;   // OTA window closes automatically after 10 min

    ota = new OtaManager(cfg);

    if (ota->begin()) {
        Serial.println(ota->updateUrl());
        Serial.println(ota->updatePassword());
    }
}

void loop() {
    if (ota && !ota->handle()) {
        ota->end();
        delete ota;
        ota = nullptr;
    }
}
```

Open `http://<device-ip>:8080/` in a browser — the identity and update link
load automatically, and the update page accepts either a firmware `.bin` or
a filesystem image.

## Two ways to start it: `begin()` vs `begin(ip)`

| | `ota->begin()` | `ota->begin(ip)` |
|---|---|---|
| Who brings the network up | OtaManager itself (STA or AP, per `Config`) | You — Ethernet already connected, or WiFi you connected yourself |
| `end()` disconnects the network | Yes | No — leaves it exactly as it was |
| `isConnected()` | Reflects real WiFi state | Always `true` |
| `onNetworkDown` fires | Yes, if the connection drops later | Never |
| Example | `BasicHttpOta`, `AccessPointOta` | `EthernetOnlyOta` |

`Config::onNetworkUp(ip)` fires once, right when the OTA service becomes
reachable, no matter which overload got it there.

## Configuration reference (`OtaManager::Config`)

| Field | Default | Meaning |
|---|---|---|
| `deviceId` | auto (chip id) | Stable per-device identifier |
| `hardwareId` | `"DEVICE"` | Model/hardware name shown on the pages |
| `swVersion` | `"1.0.0"` | Firmware version shown on the pages |
| `wifiSsid` / `wifiPassword` | — | Used only by the no-argument `begin()` |
| `asAccessPoint` | `false` | `true` = host an AP instead of joining a network |
| `wifiConnectTimeoutSec` | `15` | How long `begin()` waits for the STA link; `begin()` returns `false` if it is not up by then (`0` = don't wait, so `begin()` fails unless already connected) |
| `onNetworkUp` / `onNetworkDown` | `nullptr` | Network state callbacks, see above |
| `httpPort` | `8080` | OTA HTTP server port |
| `httpUser` | `"admin"` | HTTP Basic auth username for `/update` |
| `httpPassword` | auto-generated PIN | Leave empty to use `OtaCredentials::generatePin()` |
| `activeMinutes` | `5` | OTA window length; `0` = stays open indefinitely |
| `pageProvider` | `nullptr` → `EmbeddedPageProvider` | Swap in `FsPageProvider` for custom pages from a filesystem |
| `challengeAuth` | `true` | Enables `/PassHash.php`, `/start.php` (see below); `/getip.php`/`/identity.php` are always on |
| `onPasswordChanged` | `nullptr` | `void(const String& newPassword)`, fires whenever the PIN gets regenerated (see the gotcha below) |
| `pepper` | empty | **The actual secret** — see "Security model" below. Empty means the legacy public checksum, i.e. no real secrecy |
| `pinOptions` | see below | `maxChars` = PIN length. `startChar`/`endChar`/`stride` tune the legacy checksum and are ignored when a `pepper` is set |
| `lockoutThreshold` / `lockoutMs` / `lockoutMaxMs` | `5` / `30000` / `1800000` | Failed-auth throttling on `/update` — see "Security model" |
| `enableFtp` *(needs `OTA_ENABLE_FTP`)* | `false` | Also accept firmware over FTP |
| `ftpUser` / `ftpPassword` *(needs `OTA_ENABLE_FTP`)* | `"admin"` / empty | FTP credentials |
| `ftpDoneCallback` *(needs `OTA_ENABLE_FTP`)* | `nullptr` | `void(bool success)`, called after an FTP-triggered flash |

## The built-in pages and their endpoints

`EmbeddedPageProvider` (the default) ships the project's actual
root/upload/404 pages baked into flash — no filesystem required. Their
JavaScript talks to four endpoints on `HttpOtaProvider`, all on by default
via `challengeAuth`:

| Endpoint | Called by | Gated by `challengeAuth`? | What it does |
|---|---|---|---|
| `POST /getip.php` | root page, on load | No — always on | returns `//<ip>:<port>/update` for the "Upload Service" link |
| `POST /identity.php` | root page, on load | No — always on | returns the HW/ID/SW identity line |
| `POST /PassHash.php?Phash=xxxx` | root page, right after identity | Yes | re-derives the PIN as `generatePin(deviceId, swVersion, hardwareId, salt, pepper, pinOptions)` and echoes the salt back for display — this is what makes the effective PIN change on every page load |
| `POST /start.php` | upload page, before the real upload | Yes | no-op placeholder kept for the existing page JS |

`getip.php`/`identity.php` stay on regardless of `challengeAuth` because they
carry no secret and the root page needs them just to work at all — without
them the "Upload Service" link would never get a target.

⚠️ **Gotcha:** with `challengeAuth` on (the default), simply opening the
root page in a browser silently changes the effective password — the PIN
printed once at `begin()` can go stale the moment anyone loads `/`. Two ways
to deal with it:
- Set `onPasswordChanged` in `Config` to reprint the current PIN to Serial
  every time it changes (all examples do this).
- Set `challengeAuth = false` for one fixed PIN for the whole session — the
  root page's identity display and "Upload Service" link keep working
  exactly the same either way.

Set `challengeAuth = false` if you swap in pages that don't use this flow.
For custom-branded pages served from a filesystem instead of flash, use
`FsPageProvider` (see the `FilesystemPages` example) — it reads
`/index_root.html`, `/index_upload.html`, `/index_notfound.html` from
whichever filesystem `OTA_FS_MODE_*` selects (LittleFS by default).

## Security model

The whole point of this scheme, by design, is that **the device's own user
should never be able to compute or see the real update password themselves.**
The intended workflow:

1. A firmware `.bin` gets emailed to whoever is physically at the device —
   it's useless to anyone else without the matching password, and it stops
   being reusable the moment it's applied (the next `swVersion` invalidates
   it, see below).
2. That person opens `http://<device-ip>:8080/` and calls support, reading
   off the identity (HW/ID/SW) and the on-screen salt (the "Pass. Hash").
3. Support computes the real password from those four values using
   `tools/OTA_pass.html` (a plain offline calculator, not served by the
   device) and reads it back over the phone.
4. Once that firmware is installed and `swVersion` bumps for the next
   release, the same password stops working — the device user can't reuse
   today's credentials to skip the process next time either.

**This only works if two things hold:**

- **The pepper is real and private.** `OtaCredentials::generatePin()` is
  public source code — reading it is not a barrier to anyone.
  `Config::pepper` is the part meant to carry actual secrecy: a long,
  random, per-deployment string that both the firmware (`OtaSecrets.h` in
  your sketch, never committed — template: `tools/OtaSecrets.example.h`) and
  support's copy of `tools/OTA_pass.html` (its `Pepper` field) share. With a
  pepper set, the PIN is derived from a 32-bit FNV-1a hash of identity +
  salt + pepper (6 characters from `[0-9A-Za-z]`); with an empty pepper the
  legacy public checksum is used, whose few thousand parameter combinations
  are trivially brute-forced. The two calculators (firmware and
  `OTA_pass.html`) must be on the same library version.
- **Brute-forcing is actually expensive.** Even with a real pepper, nothing
  stops repeated guesses over the network unless something limits them —
  so `/update` locks out further attempts after `lockoutThreshold`
  consecutive failures, for `lockoutMs` (doubling on each repeat offense,
  capped at `lockoutMaxMs`). This resets on any successful auth.

With both in place, this repo can safely be **public**: the algorithm being
readable doesn't matter once the actual secret (the pepper) isn't in it.
Without a real pepper, treat the PIN as public knowledge regardless of the
repo's visibility — it's only "keep casual/accidental access out" strength,
not protection against anyone who bothered to read this source.

**Known limitations — read before deploying on an untrusted network:**

- This is a PIN scheme, not cryptography: 32 bits of state, so its strength
  rests on the secret pepper plus the lockout, not on the math.
- Everything is plain HTTP (and plain FTP): the Basic-auth PIN and the
  firmware travel unencrypted, and uploaded images are **not signed or
  verified** beyond the Update API's own checks. Use it on a trusted LAN /
  a short-lived setup AP, and keep `activeMinutes` small.
- With `challengeAuth` on, anyone who can reach the device can load `/` and
  thereby change the effective password (a nuisance/DoS against the support
  phone workflow, not a way in). Set `challengeAuth = false` for a fixed PIN.
- FTP has no PIN: set `ftpPassword` explicitly, and remember anyone with it
  can flash firmware.

## Examples

| Example | Demonstrates |
|---|---|
| `BasicHttpOta` | Minimal usage: WiFi STA + HTTP OTA, built-in pages |
| `FilesystemPages` | Custom-branded pages served from LittleFS |
| `FtpFirmwareUpload` | HTTP OTA + optional FTP firmware upload together |
| `AccessPointOta` | Device hosts its own WiFi AP instead of joining a network |
| `StandaloneComponents` | Using `WifiConnector` + `HttpOtaProvider` directly, without the `OtaManager` facade |
| `FullFeatured` | STA-with-AP-fallback + FS pages + FTP + status callbacks |
| `BootTimedApOta` | Ethernet as the permanent link; AP + OTA open for a fixed window at boot, then shut off |
| `EthernetOnlyOta` | OTA served purely over Ethernet — WiFi is never touched |
| `NetworkCallbacks` | `onNetworkUp` / `onNetworkDown` with a self-managed WiFi connection |

## Library layout

```
src/
  OtaPlatform.h          platform (ESP32/ESP8266) + filesystem compat layer
  OtaCredentials.h       device id + PIN generation (see Security model)
  WifiConnector.h        WiFi STA/AP bring-up, reusable outside OTA too
  IPageProvider.h        page-serving interface
  EmbeddedPageProvider.h default flash-resident pages (no FS dependency)
  FsPageProvider.h       optional: serve custom pages from LittleFS/SPIFFS/FFat
  HttpOtaProvider.h      the HTTP OTA server itself
  FtpFirmwareUpdater.h   optional: firmware.bin over FTP (needs OTA_ENABLE_FTP)
  OtaManager.h           facade wiring WiFi/network + HTTP OTA (+ FTP) together
  SHN_OtaLib.h           umbrella include
tools/
  OTA_pass.html          offline support-side password calculator (template, blank Pepper field)
  OtaSecrets.example.h   template for the secret pepper - copy into YOUR sketch as OtaSecrets.h
examples/                see table above
library.properties       Arduino library metadata
keywords.txt             Arduino IDE syntax highlighting
.gitignore               keeps OtaSecrets.h and any filled-in calculator copy out of git
```

Enable the FTP module by defining `OTA_ENABLE_FTP` **before** including
`SHN_OtaLib.h` (or as a global build flag):

```cpp
#define OTA_ENABLE_FTP
#include <SHN_OtaLib.h>
```

Sketches that don't define it never pull in `SimpleFTPServer` at all.

Two things to know when using FTP: `SimpleFTPServer` must be configured for
the same filesystem as `OTA_FS` (edit its `FtpServerKey.h`, e.g. LittleFS),
and — like the built-in HTML pages — the library headers are header-only, so
include `SHN_OtaLib.h` from **one** translation unit (your `.ino`) per
project.

## Troubleshooting

**"not enough space" on `/update`** — the selected Partition Scheme
(Arduino IDE: Tools → Partition Scheme) doesn't have an OTA-capable app
partition large enough for the compiled sketch. Check the compiled size in
the IDE's build output ("Sketch uses XXXXX bytes...") against the app
partition size for the chosen scheme, and pick one with OTA support and
enough room (e.g. a "with OTA" scheme rather than a "Huge APP (no OTA)" one).

**"Decryption error" printed repeatedly on Serial during upload** — by far
the most common cause: a **merged** binary (Arduino IDE's "Export Compiled
Binary", which concatenates bootloader + partition table + `boot_app0` +
app into one file for a full USB flash at offset `0x0`) was uploaded to
`/update` instead of the plain `<sketch>.ino.bin`. The merged file isn't a
single app image, so `Update.write()` runs past the app partition boundary
into unrelated flash regions, and cryptic low-level errors like this one
follow. **Always upload the plain, non-merged `.bin`** — the same file
written to the app partition offset during a normal USB upload. (If the
plain `.bin` produces this same error and flash encryption is confirmed off
at the hardware level — see `examples/CheckFlashEncryption` — that would
point elsewhere, but a merged binary is the first thing to rule out.)

## Notes

- The upload form's file input name (`firmware` vs `filesystem`) selects
  which partition gets flashed — see `HttpOtaProvider::onUpdateChunk()`.
- A successful `/update` ends with a ~1s delay then `ESP.restart()`, same as
  any standard web-OTA handler (needed to let the HTTP response reach the
  browser before reboot) — this is bounded and expected, unlike the legacy
  library's unbounded freeze/blocking behavior.
- The root page's identity/salt polling (`GetIpFunc` → `GetIdFunc` →
  `GenerateHsh`) uses self-scheduling `setTimeout` retries rather than
  `setInterval`, so it can never have more than one request in flight per
  step — the original `setInterval`-based version could, under a slow
  response, spawn an orphaned timer that kept regenerating the salt forever.

## Credits

Modular rewrite of `SHN_OtaUtil` (originally by [Sh. Nourbakhsh Rad](mailto:Sh.Nourbakhsh@gmail.com))
