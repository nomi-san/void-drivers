# Void Drivers installer

Inno Setup 7 script that packages the **Void Drivers** suite into a single `setup.exe`
for **x64 and ARM64** Windows. On the components page you pick which drivers to install
(**both checked by default**):

- **VoidDisplay** - virtual display adapter (IddCx UMDF)
- **VoidInput** - virtual HID devices: mouse / keyboard / gamepad / touch (UMDF + VHF)

The installer shows the MIT LICENSE on first open, detects the OS architecture and
installs only the matching driver build, creates each root-enumerated device node with
the bundled `devcon.exe`, installs the shared `voidctl` CLI, and optionally puts it on
`PATH`. It runs elevated (admin).

## What it installs

| Item | Location | When |
|---|---|---|
| VoidDisplay package (`.dll`/`.inf`/`.cat`) | `%ProgramFiles%\Void Drivers\display\driver` | component `display` |
| VoidInput package (`.dll`/`.inf`/`.cat`) | `%ProgramFiles%\Void Drivers\input\driver` | component `input` |
| `devcon.exe` (OS-native arch) | `%ProgramFiles%\Void Drivers\devcon` | always |
| `voidctl.exe` (OS-native arch) | `%ProgramData%\.voidrv\bin` | always |
| `display.ini` (seeded only if absent) | `%ProgramData%\.voidrv` | component `display` |
| Device nodes `Root\Void\Display` / `Root\Void\Input` | created via `devcon install` | per component |

`%ProgramData%\.voidrv` is created **writable by any logged-in user** so an unelevated
`voidctl` can persist `display.ini` without a UAC prompt (the driver only reads it, as
SYSTEM). The `bin` subfolder is re-secured read-only for non-admins (it goes on `PATH`),
so the bundled `voidctl.exe` can't be hijacked.

## Layout

```
installer/
  VoidDrivers.iss              the script
  config/display.ini           default VoidDisplay config (seeded on first install)
  redist/devcon/
    devcon-x64.exe             from the WDK (Tools\<ver>\x64\devcon.exe)
    devcon-arm64.exe           from the WDK (Tools\<ver>\arm64\devcon.exe)
  out/                         build output (gitignored)
```

To refresh `devcon`, copy it from the WDK:
`Program Files (x86)\Windows Kits\10\Tools\<ver>\{x64,arm64}\devcon.exe`.

## Build

Build both drivers and `voidctl` (x64 + ARM64) first, then compile the installer:

```pwsh
& "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDrivers.iss
```

By default it packages the **Debug** build of each component (what bring-up produces).
For distribution, build Release and pass the configs:

```pwsh
& "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDrivers.iss /DDrvConfig=Release /DCtlConfig=Release
```

Output: `installer\out\VoidDrivers-1.0.0-setup.exe`.

## Why devcon and not pnputil

`devcon install <inf> Root\Void\...` both stages the package to the driver store **and**
creates the root-enumerated device node in one step - `pnputil` alone cannot create a
root devnode. So the install path needs only the bundled `devcon.exe`. Uninstall uses the
in-box `pnputil` (no bundling) for a best-effort store cleanup after `devcon remove`.

## Caveats before shipping

- **Driver signing.** `devcon` installs a driver only if its catalog is trusted on the
  target: the dev test certificate in `Root` + `TrustedPublisher`, or an OV/EV
  Authenticode signature for release. This script does not install certificates.
- **voidctl CRT.** A *Debug* `voidctl.exe` links the debug CRT and runs only on a machine
  with the VS toolset. For distribution, ship a **Release** build (ideally statically
  linked, `/MT`) so it runs on a clean host.
