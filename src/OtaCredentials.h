//-----------------------------------------------------------------------------
// OtaCredentials.h
//
// IMPORTANT: generatePin() is NOT a cryptographic function on its own - it's
// a public, deterministic checksum. Anyone who can read this source file
// (which is everyone, once the repo is public) knows the exact algorithm.
// What actually needs to stay secret is the `pepper` argument below: a
// per-deployment string that YOU choose and that never gets committed to
// the repo. Without a pepper, or with a short/guessable one, this PIN is
// only "don't let a random device on the LAN update itself by accident"
// strength - not protection against anyone who read this file. See
// tools/OtaSecrets.example.h for how to keep the real pepper out of git.
//-----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include "OtaPlatform.h"

class OtaCredentials
{
public:
    // Tunable knobs for the checksum window. Changing these from the
    // defaults does NOT meaningfully add security on its own - the
    // combined space of reasonable values here is only a few thousand,
    // trivially brute-forced by anyone who read this file. Use `pepper`
    // (below) for actual secrecy; only touch these if you specifically
    // need to change the PIN's character set or length.
    struct PinOptions
    {
        int startChar;   // printable ASCII range
        int endChar;
        int stride;      // checksum window size
        int maxChars;    // resulting PIN length

        // An explicit constructor (rather than in-class default member
        // initializers) so PinOptions() can be used as a default argument
        // in generatePin() below without tripping GCC's "default member
        // initializer required before the end of its enclosing class"
        // error - a known quirk when a nested class's own defaults are
        // referenced from a default argument of its enclosing class.
        PinOptions(int startChar_ = 32, int endChar_ = 126,
                   int stride_ = 7, int maxChars_ = 6)
            : startChar(startChar_), endChar(endChar_),
              stride(stride_), maxChars(maxChars_)
        {}
    };

    // Stable per-device identifier (chip id / efuse mac, uppercase hex).
    static String deviceId()
    {
        return OtaPlatform_chipId();
    }

    // Deterministic PIN derived from device id + firmware version +
    // hardware id + an optional on-screen salt + a secret pepper.
    //
    // - salt: the "read a code off the screen, call support, get the
    //   matching password back" workflow - a random value shown on the
    //   device's web page so the effective PIN changes every page load.
    // - pepper: the part that actually needs to be secret. Set it to a
    //   long, random, per-deployment string kept OUT of version control
    //   (see tools/OtaSecrets.example.h). Without a real pepper, treat this PIN
    //   as public knowledge the moment this source file is - the checksum
    //   itself provides no secrecy.
    static String generatePin(const String& deviceId,
                               const String& swVersion,
                               const String& hardwareId,
                               const String& salt = "",
                               const String& pepper = "",
                               PinOptions options = PinOptions())
    {
        String input = deviceId + swVersion + salt + hardwareId + deviceId;
        input.replace(".", "");

        // The legacy checksum window below only ever reads the first ~25
        // characters of `input` (6 output chars, every 3rd position, stride
        // 7), so a pepper appended to the END of the input can never reach
        // it - the original "pepper" had no effect on the PIN at all.
        // When a pepper is set we therefore derive the PIN from a hash of
        // EVERYTHING (identity + salt + pepper) instead: FNV-1a 32-bit seed,
        // then a small LCG picks `maxChars` characters from [0-9A-Za-z].
        // pinOptions.startChar/endChar/stride are ignored in this mode.
        // With an EMPTY pepper the legacy checksum below is used unchanged
        // (PINs stay compatible with the old SHN_OtaUtil). ASCII input only.
        // Still NOT cryptographic (32 bits of state): the lockout in
        // HttpOtaProvider is what makes online guessing impractical.
        if (pepper.length())
        {
            static const char alnum[] =
                "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            uint32_t h = fnv1a32(input + pepper);
            String pin;
            for (int k = 0; k < options.maxChars; ++k)
            {
                h = h * 1664525UL + 1013904223UL;
                pin += alnum[(h >> 16) % 62];
            }
            return pin;
        }

        const int startChar   = options.startChar;
        const int endChar     = options.endChar;
        const int stride      = options.stride;
        const int maxPinChars = options.maxChars;

        String pin;
        pin.reserve(maxPinChars);

        const int len = input.length();

        for (int i = 0; i < len && (int)pin.length() < maxPinChars; ++i)
        {
            int sum = 0;
            for (int j = i; j < i + stride && j < len; ++j)
            {
                sum += (int)input[j];
            }

            sum = (sum % (endChar - startChar)) + startChar;
            sum = normalizeToAlnum(sum);

            // Keep the legacy sampling pattern: take every 3rd computed
            // value (skipping i == 0) so the PIN isn't just the first
            // few characters of the input.
            if (i > 0 && (i % 3) == 0)
            {
                pin += (char)sum;
            }
        }

        return pin;
    }

private:
    static uint32_t fnv1a32(const String& s)
    {
        uint32_t h = 2166136261UL;
        for (unsigned i = 0; i < s.length(); ++i)
        {
            h ^= (uint8_t)s[i];
            h *= 16777619UL;
        }
        return h;
    }

    // Nudges a value that landed on a non-alphanumeric ASCII code into the
    // nearest alphanumeric range, same correction table as the legacy code.
    static int normalizeToAlnum(int c)
    {
        if (c < 48)                 return c + 33;
        if (c > 57 && c < 65)       return c + 39;
        if (c > 90 && c < 97)       return c + 20;
        if (c > 122)                return c - 75;
        return c;
    }
};
