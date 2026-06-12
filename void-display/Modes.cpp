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

    // Zero-blanking timing (totalSize == activeSize), matching Parsec's VDD and
    // the IddCx samples: pixelRate = w*h*vsync, hSync = h*vsync.
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

// --- Runtime advertised-mode store ---------------------------------------

#define VOID_MAX_CUSTOM_MODES (VOIDDISPLAY_MAX_MODES - 16)  // leave room for defaults

static SRWLOCK       s_modeLock = SRWLOCK_INIT;
static VOID_MODE_DESC s_customModes[VOID_MAX_CUSTOM_MODES];
static unsigned      s_customCount = 0;

static bool SameMode(const VOID_MODE_DESC& a, UINT32 w, UINT32 h, UINT32 hz)
{
    return a.Width == w && a.Height == h && a.RefreshHz == hz;
}

static bool IsDefaultMode(UINT32 w, UINT32 h, UINT32 hz)
{
    for (unsigned i = 0; i < g_VoidDefaultModeCount; ++i) {
        if (SameMode(g_VoidDefaultModes[i], w, h, hz)) {
            return true;
        }
    }
    return false;
}

bool VoidModesAdd(UINT32 width, UINT32 height, UINT32 hz)
{
    if (width == 0 || height == 0 || hz == 0) {
        return false;
    }
    if (IsDefaultMode(width, height, hz)) {
        return true;  // already advertised
    }

    bool ok = false;
    AcquireSRWLockExclusive(&s_modeLock);
    bool dup = false;
    for (unsigned i = 0; i < s_customCount; ++i) {
        if (SameMode(s_customModes[i], width, height, hz)) { dup = true; break; }
    }
    if (dup) {
        ok = true;
    } else if (s_customCount < VOID_MAX_CUSTOM_MODES) {
        s_customModes[s_customCount].Width = width;
        s_customModes[s_customCount].Height = height;
        s_customModes[s_customCount].RefreshHz = hz;
        s_customCount++;
        ok = true;
    }
    ReleaseSRWLockExclusive(&s_modeLock);
    return ok;
}

bool VoidModesRemove(UINT32 width, UINT32 height, UINT32 hz)
{
    bool removed = false;
    AcquireSRWLockExclusive(&s_modeLock);
    for (unsigned i = 0; i < s_customCount; ++i) {
        if (SameMode(s_customModes[i], width, height, hz)) {
            for (unsigned j = i + 1; j < s_customCount; ++j) {
                s_customModes[j - 1] = s_customModes[j];
            }
            s_customCount--;
            removed = true;
            break;
        }
    }
    ReleaseSRWLockExclusive(&s_modeLock);
    return removed;
}

unsigned VoidModesGet(VOID_MODE_DESC* out, unsigned cap)
{
    unsigned n = 0;
    for (unsigned i = 0; i < g_VoidDefaultModeCount && n < cap; ++i) {
        out[n++] = g_VoidDefaultModes[i];
    }
    AcquireSRWLockShared(&s_modeLock);
    for (unsigned i = 0; i < s_customCount && n < cap; ++i) {
        out[n++] = s_customModes[i];
    }
    ReleaseSRWLockShared(&s_modeLock);
    return n;
}

unsigned VoidModesCount(void)
{
    AcquireSRWLockShared(&s_modeLock);
    unsigned n = g_VoidDefaultModeCount + s_customCount;
    ReleaseSRWLockShared(&s_modeLock);
    return n;
}
