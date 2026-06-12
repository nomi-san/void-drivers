#include "voidrv.h"

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <new>
#include <wchar.h>

// Both control-interface headers are self-contained and each #include <winioctl.h>,
// whose device-interface GUIDs are emitted on EVERY include (outside its include
// guard). Pulling the headers here - before INITGUID - leaves all of those GUIDs as
// plain extern declarations. We then allocate storage for ONLY the two interface
// GUIDs we actually use. (Including either Public.h after INITGUID would re-emit
// winioctl's GUIDs with initializers and allocate them twice -> C2374.) Both files
// are named Public.h, so void-input is included by explicit relative path.
#include "Public.h"                  // void-display control interface
#include "../void-input/Public.h"    // void-input control interface

#include <initguid.h>
DEFINE_GUID(GUID_DEVINTERFACE_VOIDDISPLAY,
    0x40255101, 0xa910, 0x441c, 0x84, 0xd6, 0x9f, 0x02, 0x71, 0x97, 0xfa, 0x70);
DEFINE_GUID(GUID_DEVINTERFACE_VOIDINPUT,
    0x7b0c8d49, 0x54fc, 0x45ca, 0xab, 0x47, 0x6a, 0x57, 0x2f, 0x2f, 0xf5, 0x10);

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

struct VoidrvDisplay {
    HANDLE Device;
};

// Enumerate the device interface and open the first instance.
static HANDLE OpenInterface(const GUID* interfaceGuid)
{
    HANDLE handle = INVALID_HANDLE_VALUE;

    HDEVINFO devInfo = SetupDiGetClassDevsW(
        interfaceGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    SP_DEVICE_INTERFACE_DATA ifData;
    ZeroMemory(&ifData, sizeof(ifData));
    ifData.cbSize = sizeof(ifData);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, interfaceGuid, i, &ifData); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, nullptr, 0, &needed, nullptr);
        if (needed == 0) {
            continue;
        }

        auto* detail = static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(malloc(needed));
        if (!detail) {
            continue;
        }
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detail, needed, &needed, nullptr)) {
            handle = CreateFileW(detail->DevicePath,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        free(detail);

        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return handle;
}

static bool Control(HANDLE device, DWORD code,
                    void* in, DWORD inLen, void* out, DWORD outLen, DWORD* bytes)
{
    DWORD br = 0;
    BOOL ok = DeviceIoControl(device, code, in, inLen, out, outLen, &br, nullptr);
    if (bytes) {
        *bytes = br;
    }
    return ok != FALSE;
}

extern "C" {

VoidrvStatus VoidrvDisplayQueryStatus(void)
{
    HANDLE h = OpenInterface(&GUID_DEVINTERFACE_VOIDDISPLAY);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        // Distinguish "no interface" is not trivial here; report not-installed.
        return VOIDRV_STATUS_NOT_INSTALLED;
    }
    CloseHandle(h);
    return VOIDRV_STATUS_OK;
}

VoidrvDisplayHandle VoidrvDisplayOpen(void)
{
    HANDLE h = OpenInterface(&GUID_DEVINTERFACE_VOIDDISPLAY);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return nullptr;
    }
    auto* d = new (std::nothrow) VoidrvDisplay();
    if (!d) {
        CloseHandle(h);
        return nullptr;
    }
    d->Device = h;
    return d;
}

void VoidrvDisplayClose(VoidrvDisplayHandle handle)
{
    if (!handle) {
        return;
    }
    if (handle->Device && handle->Device != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->Device);
    }
    delete handle;
}

uint32_t VoidrvDisplayVersion(VoidrvDisplayHandle handle)
{
    if (!handle) {
        return 0;
    }
    ULONG version = 0;
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_VERSION, nullptr, 0,
                 &version, sizeof(version), nullptr)) {
        return 0;
    }
    return version;
}

static bool VoidApplyMode(uint32_t index, const VoidrvDisplayMode* mode);
static void PersistDisplay(uint32_t index, const VoidrvDisplayMode* mode, bool present);

int VoidrvDisplayAdd(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode)
{
    if (!handle) {
        return -1;
    }
    VOIDDISPLAY_MODE wire;
    ZeroMemory(&wire, sizeof(wire));
    if (mode) {
        wire.Width = mode->Width;
        wire.Height = mode->Height;
        wire.RefreshHz = mode->RefreshHz;
    }
    ULONG index = 0;
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_ADD, &wire, sizeof(wire),
                 &index, sizeof(index), nullptr)) {
        return -1;
    }

    // If the display auto-activates, force the requested mode over whatever the OS
    // remembered for this monitor identity. Best-effort: poll briefly for it to go
    // active (it does nothing if the display stays inactive).
    if (mode && mode->Width && mode->Height && mode->RefreshHz) {
        for (int tries = 0; tries < 10; ++tries) {
            if (VoidApplyMode((uint32_t)index, mode)) {
                break;
            }
            Sleep(150);
        }
    }

    // Persist for restore-on-start. Record the effective mode (the driver's
    // default when the request was zeroed).
    VoidrvDisplayMode eff = { 1920, 1080, 60 };
    if (mode && mode->Width && mode->Height && mode->RefreshHz) {
        eff = *mode;
    }
    PersistDisplay((uint32_t)index, &eff, true);
    return (int)index;
}

bool VoidrvDisplayRemove(VoidrvDisplayHandle handle, uint32_t index)
{
    if (!handle) {
        return false;
    }
    ULONG i = index;
    bool ok = Control(handle->Device, IOCTL_VOIDDISPLAY_REMOVE, &i, sizeof(i), nullptr, 0, nullptr);
    if (ok) {
        PersistDisplay(index, nullptr, false);
    }
    return ok;
}

// Apply a mode to the index-th active VoidDisplay monitor via the GDI display
// config. Windows remembers the per-monitor mode in the registry, so the driver's
// advertised mode is not enough on a re-add - we force the requested mode here.
// Identifies VoidDisplay GDI devices by the "VVD" EDID id on their monitor child.
// (Index is matched against active VoidDisplay devices in enumeration order; exact
//  for the common single-display case.)
static bool VoidApplyMode(uint32_t index, const VoidrvDisplayMode* mode)
{
    WCHAR devices[VOIDRV_MAX_DISPLAYS][32];
    int count = 0;

    DISPLAY_DEVICEW dd;
    ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);
    for (DWORD i = 0; count < VOIDRV_MAX_DISPLAYS && EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            DISPLAY_DEVICEW mon;
            ZeroMemory(&mon, sizeof(mon));
            mon.cb = sizeof(mon);
            if (EnumDisplayDevicesW(dd.DeviceName, 0, &mon, 0) && wcsstr(mon.DeviceID, L"VVD")) {
                lstrcpynW(devices[count], dd.DeviceName, 32);
                ++count;
            }
        }
        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
    }

    if (index >= (uint32_t)count) {
        return false;  // that VoidDisplay isn't active (no GDI device to drive)
    }

    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsW(devices[index], ENUM_CURRENT_SETTINGS, &dm)) {
        return false;
    }
    dm.dmPelsWidth        = mode->Width;
    dm.dmPelsHeight       = mode->Height;
    dm.dmDisplayFrequency = mode->RefreshHz;
    dm.dmFields           = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

    LONG rc = ChangeDisplaySettingsExW(devices[index], &dm, nullptr, CDS_UPDATEREGISTRY, nullptr);
    return rc == DISP_CHANGE_SUCCESSFUL;
}

bool VoidrvDisplaySetMode(VoidrvDisplayHandle handle, uint32_t index, const VoidrvDisplayMode* mode)
{
    if (!handle || !mode) {
        return false;
    }
    // Update the driver's stored mode (keeps LIST accurate)...
    VOIDDISPLAY_SET_MODE wire;
    ZeroMemory(&wire, sizeof(wire));
    wire.Index = index;
    wire.Mode.Width = mode->Width;
    wire.Mode.Height = mode->Height;
    wire.Mode.RefreshHz = mode->RefreshHz;
    Control(handle->Device, IOCTL_VOIDDISPLAY_SET_MODE, &wire, sizeof(wire), nullptr, 0, nullptr);

    // ...then apply the actual OS-level resolution change.
    bool applied = VoidApplyMode(index, mode);

    // Persist the new mode so restore-on-start brings it back.
    PersistDisplay(index, mode, true);
    return applied;
}

bool VoidrvDisplayList(VoidrvDisplayHandle handle, VoidrvDisplayState* state)
{
    if (!handle || !state) {
        return false;
    }
    VOIDDISPLAY_STATE wire;
    ZeroMemory(&wire, sizeof(wire));
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_LIST, nullptr, 0,
                 &wire, sizeof(wire), nullptr)) {
        return false;
    }

    state->Count = wire.Count;
    for (uint32_t i = 0; i < VOIDRV_MAX_DISPLAYS && i < VOIDDISPLAY_MAX_DISPLAYS; ++i) {
        state->Entries[i].InUse = wire.Entries[i].InUse;
        state->Entries[i].Mode.Width = wire.Entries[i].Mode.Width;
        state->Entries[i].Mode.Height = wire.Entries[i].Mode.Height;
        state->Entries[i].Mode.RefreshHz = wire.Entries[i].Mode.RefreshHz;
    }
    return true;
}

// The driver re-reads its custom-mode list from this key at adapter init, so the
// IOCTL only changes the live list - we mirror the change here for persistence.
static const wchar_t kVoidParamsKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WUDF\\Services\\VoidDisplay\\Parameters";

// Mirror a custom-mode add/remove into the driver's Parameters key so it survives
// a device restart / reboot. Stored as a REG_BINARY array of packed
// VoidrvDisplayMode triples. Best-effort: writing HKLM needs elevation, and a
// failure here does not undo the live (IOCTL) change.
static void PersistCustomMode(const VoidrvDisplayMode* mode, bool add)
{
    VoidrvDisplayMode list[VOIDRV_MAX_MODES];
    DWORD cb = sizeof(list);
    DWORD count = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kVoidParamsKey, L"CustomModes",
                     RRF_RT_REG_BINARY, nullptr, list, &cb) == ERROR_SUCCESS) {
        count = cb / (DWORD)sizeof(VoidrvDisplayMode);
    }

    DWORD found = count;
    for (DWORD i = 0; i < count; ++i) {
        if (list[i].Width == mode->Width && list[i].Height == mode->Height &&
            list[i].RefreshHz == mode->RefreshHz) {
            found = i;
            break;
        }
    }

    if (add) {
        if (found != count || count >= VOIDRV_MAX_MODES) {
            return;  // already present, or full
        }
        list[count++] = *mode;
    } else {
        if (found == count) {
            return;  // not present
        }
        for (DWORD i = found + 1; i < count; ++i) {
            list[i - 1] = list[i];
        }
        --count;
    }

    RegSetKeyValueW(HKEY_LOCAL_MACHINE, kVoidParamsKey, L"CustomModes",
                    REG_BINARY, list, count * (DWORD)sizeof(VoidrvDisplayMode));
}

// One persisted display, matching the driver's VoidPersistEntry layout.
struct VoidrvPersistEntry {
    uint32_t Index;
    uint32_t Width;
    uint32_t Height;
    uint32_t RefreshHz;
};

// Mirror a display add / setmode / remove into the driver's Parameters key so the
// driver recreates it at init (RestoreOnStart). Stored as a REG_BINARY array keyed
// by slot index. Best-effort: the HKLM write needs elevation and a failure does
// not affect the live operation.
static void PersistDisplay(uint32_t index, const VoidrvDisplayMode* mode, bool present)
{
    VoidrvPersistEntry list[VOIDRV_MAX_DISPLAYS];
    DWORD cb = sizeof(list);
    DWORD count = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kVoidParamsKey, L"PersistedDisplays",
                     RRF_RT_REG_BINARY, nullptr, list, &cb) == ERROR_SUCCESS) {
        count = cb / (DWORD)sizeof(VoidrvPersistEntry);
    }

    DWORD found = count;
    for (DWORD i = 0; i < count; ++i) {
        if (list[i].Index == index) {
            found = i;
            break;
        }
    }

    if (present) {
        if (found == count) {
            if (count >= VOIDRV_MAX_DISPLAYS) {
                return;  // full
            }
            list[count].Index = index;
            found = count;
            ++count;
        }
        list[found].Width     = mode->Width;
        list[found].Height    = mode->Height;
        list[found].RefreshHz = mode->RefreshHz;
    } else {
        if (found == count) {
            return;  // not present
        }
        for (DWORD i = found + 1; i < count; ++i) {
            list[i - 1] = list[i];
        }
        --count;
    }

    RegSetKeyValueW(HKEY_LOCAL_MACHINE, kVoidParamsKey, L"PersistedDisplays",
                    REG_BINARY, list, count * (DWORD)sizeof(VoidrvPersistEntry));
}

bool VoidrvDisplayAddMode(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode)
{
    if (!handle || !mode) {
        return false;
    }
    VOIDDISPLAY_MODE wire;
    ZeroMemory(&wire, sizeof(wire));
    wire.Width = mode->Width;
    wire.Height = mode->Height;
    wire.RefreshHz = mode->RefreshHz;
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_ADD_MODE, &wire, sizeof(wire), nullptr, 0, nullptr)) {
        return false;
    }
    PersistCustomMode(mode, true);
    return true;
}

bool VoidrvDisplayRemoveMode(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode)
{
    if (!handle || !mode) {
        return false;
    }
    VOIDDISPLAY_MODE wire;
    ZeroMemory(&wire, sizeof(wire));
    wire.Width = mode->Width;
    wire.Height = mode->Height;
    wire.RefreshHz = mode->RefreshHz;
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_REMOVE_MODE, &wire, sizeof(wire), nullptr, 0, nullptr)) {
        return false;
    }
    PersistCustomMode(mode, false);
    return true;
}

bool VoidrvDisplayPersistenceWritable(void)
{
    // Probe write access to the Parameters key without side effects: opening for
    // KEY_SET_VALUE succeeds only when the caller is elevated (the persistence
    // writes go to the same key).
    HKEY hk = nullptr;
    LSTATUS rs = RegOpenKeyExW(HKEY_LOCAL_MACHINE, kVoidParamsKey, 0, KEY_SET_VALUE, &hk);
    if (rs == ERROR_SUCCESS) {
        RegCloseKey(hk);
        return true;
    }
    return false;
}

bool VoidrvDisplayListModes(VoidrvDisplayHandle handle, VoidrvModeList* list)
{
    if (!handle || !list) {
        return false;
    }
    VOIDDISPLAY_MODE_LIST wire;
    ZeroMemory(&wire, sizeof(wire));
    if (!Control(handle->Device, IOCTL_VOIDDISPLAY_LIST_MODES, nullptr, 0,
                 &wire, sizeof(wire), nullptr)) {
        return false;
    }

    list->Count = wire.Count;
    for (uint32_t i = 0; i < VOIDRV_MAX_MODES && i < VOIDDISPLAY_MAX_MODES; ++i) {
        list->Modes[i].Width = wire.Modes[i].Width;
        list->Modes[i].Height = wire.Modes[i].Height;
        list->Modes[i].RefreshHz = wire.Modes[i].RefreshHz;
    }
    return true;
}

// ===================== VoidInput =====================================

// Mouse report ids - match the VoidInput mouse HID descriptor (void-input/Devices.cpp).
#define VOIDRV_MOUSE_RID_RELATIVE 1
#define VOIDRV_MOUSE_RID_ABSOLUTE 2

struct VoidrvInput {
    HANDLE          Device;
    VoidrvInputType Type;
};

VoidrvStatus VoidrvInputQueryStatus(void)
{
    HANDLE h = OpenInterface(&GUID_DEVINTERFACE_VOIDINPUT);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return VOIDRV_STATUS_NOT_INSTALLED;
    }
    CloseHandle(h);
    return VOIDRV_STATUS_OK;
}

uint32_t VoidrvInputVersion(void)
{
    HANDLE h = OpenInterface(&GUID_DEVINTERFACE_VOIDINPUT);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return 0;
    }
    ULONG version = 0;
    bool ok = Control(h, IOCTL_VOIDINPUT_VERSION, nullptr, 0, &version, sizeof(version), nullptr);
    CloseHandle(h);
    return ok ? version : 0;
}

VoidrvInputHandle VoidrvInputCreate(VoidrvInputType type)
{
    HANDLE h = OpenInterface(&GUID_DEVINTERFACE_VOIDINPUT);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return nullptr;
    }

    VOIDINPUT_CREATE req;
    ZeroMemory(&req, sizeof(req));
    req.Type = (UINT32)type;
    if (!Control(h, IOCTL_VOIDINPUT_CREATE, &req, sizeof(req), nullptr, 0, nullptr)) {
        CloseHandle(h);
        return nullptr;
    }

    auto* d = new (std::nothrow) VoidrvInput();
    if (!d) {
        CloseHandle(h);   // also removes the device
        return nullptr;
    }
    d->Device = h;
    d->Type   = type;
    return d;
}

void VoidrvInputClose(VoidrvInputHandle handle)
{
    if (!handle) {
        return;
    }
    if (handle->Device && handle->Device != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->Device);   // closing the handle removes the device
    }
    delete handle;
}

// Pack and submit one 8-byte mouse report (report-id + buttons + X + Y + wheels).
// X/Y are little-endian 16-bit; the driver interprets them per the report id
// (relative = signed delta, absolute = 0..32767).
static bool MouseSubmit(VoidrvInputHandle handle, uint8_t reportId, uint8_t buttons,
                        uint16_t x, uint16_t y, int8_t wheel, int8_t hwheel)
{
    if (!handle || handle->Type != VOIDRV_INPUT_MOUSE ||
        handle->Device == INVALID_HANDLE_VALUE) {
        return false;
    }
    uint8_t report[8];
    report[0] = reportId;
    report[1] = buttons;
    report[2] = (uint8_t)(x & 0xFF);
    report[3] = (uint8_t)(x >> 8);
    report[4] = (uint8_t)(y & 0xFF);
    report[5] = (uint8_t)(y >> 8);
    report[6] = (uint8_t)wheel;
    report[7] = (uint8_t)hwheel;

    DWORD written = 0;
    return WriteFile(handle->Device, report, sizeof(report), &written, nullptr) != FALSE;
}

bool VoidrvInputMouseMoveRelative(VoidrvInputHandle handle, int16_t dx, int16_t dy,
                                  uint8_t buttons, int8_t wheel, int8_t hwheel)
{
    return MouseSubmit(handle, VOIDRV_MOUSE_RID_RELATIVE, buttons,
                       (uint16_t)dx, (uint16_t)dy, wheel, hwheel);
}

bool VoidrvInputMouseMoveAbsolute(VoidrvInputHandle handle, uint16_t x, uint16_t y,
                                  uint8_t buttons, int8_t wheel, int8_t hwheel)
{
    return MouseSubmit(handle, VOIDRV_MOUSE_RID_ABSOLUTE, buttons, x, y, wheel, hwheel);
}

} // extern "C"
