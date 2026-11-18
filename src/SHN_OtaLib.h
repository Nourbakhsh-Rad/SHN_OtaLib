//-----------------------------------------------------------------------------
// SHN_OtaLib.h - single include for sketches.
//
// Define OTA_ENABLE_FTP before this include (or as a build flag) if you need
// the FTP firmware-upload module; it's excluded by default so projects that
// don't need it don't pay for SimpleFTPServer at all.
//-----------------------------------------------------------------------------
#pragma once

#include "OtaPlatform.h"
#include "OtaCredentials.h"
#include "WifiConnector.h"
#include "IPageProvider.h"
#include "EmbeddedPageProvider.h"
#include "FsPageProvider.h"
#include "HttpOtaProvider.h"
#include "OtaManager.h"

#ifdef OTA_ENABLE_FTP
    #include "FtpFirmwareUpdater.h"
#endif
