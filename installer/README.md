# VoidDisplay installer

Inno Setup 7 script that packages the **Void Virtual Display Adapter** driver into a
single `setup.exe` for **x64 and ARM64** Windows. The installer detects the OS
architecture, installs the matching driver, creates the device node with the bundled
`devcon.exe`, lays down the operator config, and optionally puts `voidctl` on `PATH`.

## What it installs

| Item | Location |
|---|---|
| Driver package (`.dll` / `.inf` / `.cat`, OS-native arch) | `%ProgramFiles%\Void\VoidDisplay\driver` |
| `devcon.exe` (OS-native arch) | `%ProgramFiles%\Void\VoidDisplay\devcon` |
| `voidctl.exe` CLI | `%ProgramData%\.voidrv\bin` |
| `display.ini` (seeded only if absent) | `%ProgramData%\.voidrv` |
| Device node `Root\Void\Display` | created via `devcon install` |

`%ProgramData%\.voidrv` is created **writable by any logged-in user** so an unelevated
`voidctl` can persist `display.ini` without a UAC prompt (the driver only reads it, as
SYSTEM). The `bin` subfolder is re-secured read-only for non-admins (it sits on `PATH`),
so the bundled `voidctl.exe` can't be hijacked.

## Layout

```
installer/
  VoidDisplay.iss              the script
  config/display.ini           default config template (seeded on first install)
  redist/devcon/
    devcon-x64.exe             from the WDK (Tools\<ver>\x64\devcon.exe)
    devcon-arm64.exe           from the WDK (Tools\<ver>\arm64\devcon.exe)
  out/                         build output (gitignored)
```

If you need to refresh `devcon`, copy it from the WDK:
`Program Files (x86)\Windows Kits\10\Tools\<ver>\{x64,arm64}\devcon.exe`.

## Build

Build the driver (both arches) and `voidctl` first, then compile the installer:

```pwsh
& "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDisplay.iss
```

By default it packages the **Debug** build of each component (what bring-up produces).
For distribution, build Release and pass the configs:

```pwsh
& "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDisplay.iss /DDrvConfig=Release /DCtlConfig=Release
```

Output: `installer\out\VoidDisplay-1.0.0-setup.exe`.

## Why devcon and not pnputil

`devcon install <inf> Root\Void\Display` both stages the package to the driver store
**and** creates the root-enumerated device node in one step - `pnputil` alone cannot
create a root devnode. So the install path needs only the bundled `devcon.exe`. Uninstall
uses the in-box `pnputil` (no bundling) for a best-effort store cleanup after
`devcon remove`.

## Caveats before shipping

- **Driver signing.** `devcon` installs the driver only if its catalog is trusted on the
  target: the dev test certificate in `Root` + `TrustedPublisher`, or an OV/EV
  Authenticode signature for release. This script does not install certificates.
- **voidctl CRT.** A *Debug* `voidctl.exe` links the debug CRT and runs only on a machine
  with the VS toolset. For distribution, ship a **Release** build (ideally statically
  linked, `/MT`) so it runs on a clean host.
- **ARM64 voidctl.** Only an x64 `voidctl` exists today; it runs under x64 emulation on
  ARM64. Drop in an ARM64-native build later if needed.
