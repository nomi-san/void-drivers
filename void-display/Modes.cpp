#include "Modes.h"

// Default mode set advertised on every VoidDisplay monitor. The first entry is
// the default (1920x1080 @ 60). Refresh-rate variants are listed as separate
// entries so they each become selectable modes.
const VOID_MODE_DESC g_VoidDefaultModes[] = {
    { 1920, 1080,  60 },   // default - keep first
    { 1920, 1080, 120 },
    { 1920, 1080, 144 },
    { 1920, 1080, 240 },
    { 1280,  720,  60 },
    { 1600,  900,  60 },
    { 1920, 1200,  60 },
    { 2560, 1440,  60 },
    { 2560, 1440, 120 },
    { 2560, 1440, 144 },
    { 2560, 1600,  60 },
    { 3440, 1440,  60 },
    { 3440, 1440, 100 },
    { 3840, 2160,  60 },
    { 3840, 2160, 120 },
};

const unsigned g_VoidDefaultModeCount =
    sizeof(g_VoidDefaultModes) / sizeof(g_VoidDefaultModes[0]);

DISPLAYCONFIG_VIDEO_SIGNAL_INFO VoidCreateSignalInfo(UINT32 width, UINT32 height,
                                                     UINT32 vsync, UINT32 vSyncFreqDivider)
{
    DISPLAYCONFIG_VIDEO_SIGNAL_INFO info = {};

    // No blanking is modeled; for an indirect display the active geometry and
    // the rational refresh rates are what matter to the OS.
    const UINT64 totalPixels = (UINT64)width * (UINT64)height;

    info.pixelRate              = totalPixels * (UINT64)vsync;
    info.hSyncFreq.Numerator    = (UINT32)((UINT64)height * (UINT64)vsync);
    info.hSyncFreq.Denominator  = 1;
    info.vSyncFreq.Numerator    = vsync;
    info.vSyncFreq.Denominator  = 1;
    info.activeSize.cx          = width;
    info.activeSize.cy          = height;
    info.totalSize.cx           = width;
    info.totalSize.cy           = height;

    // videoStandard = D3DKMDT_VSS_OTHER (255). vSyncFreqDivider must be 0 for a
    // monitor mode and non-zero for a target mode (caller decides).
    info.AdditionalSignalInfo.videoStandard    = 255;
    info.AdditionalSignalInfo.vSyncFreqDivider = vSyncFreqDivider;

    info.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    return info;
}

unsigned VoidFindModeIndex(UINT32 width, UINT32 height, UINT32 vsync)
{
    for (unsigned i = 0; i < g_VoidDefaultModeCount; ++i) {
        if (g_VoidDefaultModes[i].Width == width &&
            g_VoidDefaultModes[i].Height == height &&
            g_VoidDefaultModes[i].RefreshHz == vsync) {
            return i;
        }
    }
    return 0;
}
