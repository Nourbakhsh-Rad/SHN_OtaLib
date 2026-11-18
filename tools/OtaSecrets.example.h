//-----------------------------------------------------------------------------
// OtaSecrets.example.h
//
// This is a TEMPLATE. Copy it into YOUR sketch/project folder as OtaSecrets.h
// (NOT into the library folder - a library update would delete it) and fill
// in a real value below. A file named OtaSecrets.h is already in .gitignore. OtaSecrets.h itself must never be
// committed - it's the one thing that actually needs to stay out of the
// repo, public or private.
//
// Why this matters: OtaCredentials::generatePin() is public source code.
// With an empty pepper, anyone who reads this repo can compute a device's
// OTA password from the HW/ID/SW/salt values its own web page already shows
// them. The pepper is the only secret input to the scheme.
//
// Pick something long and random - e.g. generate 24+ random characters
// with a password manager or `openssl rand -base64 24` - and use the SAME
// value both here (compiled into the device firmware) and in the support
// team's copy of tools/OTA_pass.html (its own "_pepper" field), since both
// sides must compute the identical PIN.
//-----------------------------------------------------------------------------
#pragma once

#define OTA_SECRET_PEPPER "REPLACE_ME_WITH_A_LONG_RANDOM_STRING"
