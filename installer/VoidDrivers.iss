; ===========================================================================
;  VoidDrivers.iss - Inno Setup 7 installer for the Void Drivers suite
; ===========================================================================
;
;  One installer for BOTH x64 and ARM64 Windows that can install either or both
;  Void virtual drivers (default: both):
;    * VoidDisplay - virtual display adapter (IddCx UMDF)
;    * VoidInput   - virtual HID devices: mouse/keyboard/gamepad/touch (UMDF + VHF)
;
;  It detects the OS architecture and lays down the matching driver packages,
;  creates each root-enumerated device node with the bundled devcon.exe, installs
;  the shared voidctl CLI into C:\ProgramData\.voidrv\bin (optionally on PATH),
;  seeds the display config folder, and runs elevated (admin).
;
;  Build:
;    "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDrivers.iss
;  Package Release builds for distribution:
;    ... /DDrvConfig=Release /DCtlConfig=Release
;
;  NOTE on signing: devcon installs a driver only if its catalog (.cat) is trusted
;  on the target - the dev test certificate in Root + TrustedPublisher, or an OV/EV
;  Authenticode signature for release. This script does not manage certificates.
; ===========================================================================

#define AppName       "Void Drivers"
#define AppPublisher  "Void Virtual Driver"
#define AppVersion    "1.0.0"

; Hardware IDs of the two root-enumerated drivers.
#define HwidDisplay   "Root\Void\Display"
#define HwidInput     "Root\Void\Input"

; Which build configuration of each component to package. Override on the ISCC
; command line, e.g. /DDrvConfig=Release /DCtlConfig=Release.
#ifndef DrvConfig
  #define DrvConfig "Debug"
#endif
#ifndef CtlConfig
  #define CtlConfig "Debug"
#endif

; Source trees, relative to this .iss (which lives in installer\).
#define DispX64   "..\void-display\x64\"   + DrvConfig + "\VoidDisplay"
#define DispArm64 "..\void-display\ARM64\" + DrvConfig + "\VoidDisplay"
#define InpX64    "..\void-input\x64\"     + DrvConfig + "\VoidInput"
#define InpArm64  "..\void-input\ARM64\"   + DrvConfig + "\VoidInput"
#define CtlX64    "..\voidctl\x64\"         + CtlConfig + "\voidctl.exe"
#define CtlArm64  "..\voidctl\ARM64\"       + CtlConfig + "\voidctl.exe"

[Setup]
AppId={{0ADED836-350E-4170-BED0-E759464EB3A0}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Void Drivers
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
; Build a 64-bit (x64) Setup so [Code] runs as a 64-bit process: {sys} is the real
; System32 and the 64-bit-only pnputil.exe is reachable (a 32-bit Setup is WOW64-
; redirected to SysWOW64, where pnputil does not exist). The x64 Setup runs natively
; on x64 and under emulation on ARM64; the driver payload is still arch-selected per
; OS below. (Inno Setup 7 removed EnableFsRedirection in favor of a 64-bit Setup.)
SetupArchitecture=x64
; x64 and ARM64 only - there is no 32-bit x86 driver, so refuse on plain x86.
ArchitecturesAllowed=x64compatible or arm64
ArchitecturesInstallIn64BitMode=x64compatible or arm64
Compression=lzma2/max
WizardStyle=modern
ChangesEnvironment=yes
UninstallDisplayName={#AppName}
OutputDir=out
OutputBaseFilename=VoidDrivers-{#AppVersion}-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Both drivers (recommended)"
Name: "custom"; Description: "Choose drivers"; Flags: iscustom

[Components]
Name: "display"; Description: "VoidDisplay - virtual display adapter"; Types: full
Name: "input";   Description: "VoidInput - virtual HID devices (mouse, keyboard, gamepad, touch)"; Types: full

[Tasks]
Name: "addtopath"; Description: "Add voidctl to PATH"; GroupDescription: "Command-line tools:"

[Dirs]
; Config folder, writable by any logged-in user so an UNELEVATED voidctl can persist
; display.ini with no UAC prompt (the driver only READS it, as SYSTEM). voidctl.exe
; itself lives in {app}\bin (Program Files, admin-only writable), so nothing
; world-writable lands on PATH - no ACL hardening needed.
Name: "{commonappdata}\.voidrv"; Permissions: everyone-modify; Components: display

[Files]
; ---- VoidDisplay driver (component: display; only the OS-native arch installs) ----
Source: "{#DispArm64}\VoidDisplay.dll"; DestDir: "{app}\display\driver"; Components: display; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DispArm64}\VoidDisplay.inf"; DestDir: "{app}\display\driver"; Components: display; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DispArm64}\voiddisplay.cat"; DestDir: "{app}\display\driver"; Components: display; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DispX64}\VoidDisplay.dll";   DestDir: "{app}\display\driver"; Components: display; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#DispX64}\VoidDisplay.inf";   DestDir: "{app}\display\driver"; Components: display; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#DispX64}\voiddisplay.cat";   DestDir: "{app}\display\driver"; Components: display; Check: PreferX64Files;   Flags: ignoreversion

; ---- VoidInput driver (component: input; only the OS-native arch installs) ----
Source: "{#InpArm64}\VoidInput.dll"; DestDir: "{app}\input\driver"; Components: input; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#InpArm64}\VoidInput.inf"; DestDir: "{app}\input\driver"; Components: input; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#InpArm64}\voidinput.cat"; DestDir: "{app}\input\driver"; Components: input; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#InpX64}\VoidInput.dll";   DestDir: "{app}\input\driver"; Components: input; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#InpX64}\VoidInput.inf";   DestDir: "{app}\input\driver"; Components: input; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#InpX64}\voidinput.cat";   DestDir: "{app}\input\driver"; Components: input; Check: PreferX64Files;   Flags: ignoreversion

; ---- devcon.exe (OS-native arch; shared, flattened names dodge x64/ ARM64/ ignores) ----
Source: "redist\devcon\devcon-arm64.exe"; DestDir: "{app}\devcon"; DestName: "devcon.exe"; Check: PreferArm64Files; Flags: ignoreversion
Source: "redist\devcon\devcon-x64.exe";   DestDir: "{app}\devcon"; DestName: "devcon.exe"; Check: PreferX64Files;   Flags: ignoreversion

; ---- voidctl CLI -> {app}\bin (Program Files; OS-native arch; shared by both) ----
Source: "{#CtlArm64}"; DestDir: "{app}\bin"; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#CtlX64}";   DestDir: "{app}\bin"; Check: PreferX64Files;   Flags: ignoreversion

; ---- default display config: seeded only if absent, preserved on upgrade/uninstall ----
Source: "config\display.ini"; DestDir: "{commonappdata}\.voidrv"; Components: display; Flags: onlyifdoesntexist uninsneveruninstall

[Code]
const
  EnvKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

var
  gNeedRestart: Boolean;

{ ---- architecture selection (Inno's three-arch pattern) -------------------- }
function PreferArm64Files: Boolean;
begin
  Result := IsArm64;
end;

function PreferX64Files: Boolean;
begin
  Result := not IsArm64 and IsX64Compatible;
end;

function BinDir: String;
begin
  Result := ExpandConstant('{app}\bin');
end;

{ ---- system PATH (HKLM) add/remove ----------------------------------------- }
procedure EnvAddPath(Path: string);
var
  Paths: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvKey, 'Path', Paths) then
    Paths := '';
  if Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';') > 0 then
    exit;
  if (Paths <> '') and (Paths[Length(Paths)] <> ';') then
    Paths := Paths + ';';
  Paths := Paths + Path + ';';
  RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvKey, 'Path', Paths);
end;

procedure EnvRemovePath(Path: string);
var
  Paths: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvKey, 'Path', Paths) then
    exit;
  P := Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';');
  if P = 0 then
    exit;
  Delete(Paths, P - 1, Length(Path) + 1);
  RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvKey, 'Path', Paths);
end;

{ ---- devnode + store helpers (shared by clean install and uninstall) -------- }
procedure RemoveDevnode(Hwid: String);
var
  devcon: String;
  rc: Integer;
begin
  devcon := ExpandConstant('{app}\devcon\devcon.exe');
  if FileExists(devcon) then
    Exec(devcon, 'remove ' + Hwid, '', SW_HIDE, ewWaitUntilTerminated, rc);
end;

procedure DeleteStorePackage(OrigInfLower: String);
var
  tmp, pub, low: String;
  lines: TArrayOfString;
  i, rc: Integer;
begin
  tmp := ExpandConstant('{tmp}\vd_enum.txt');
  Exec(ExpandConstant('{cmd}'), '/C pnputil /enum-drivers > "' + tmp + '"',
       '', SW_HIDE, ewWaitUntilTerminated, rc);
  if not LoadStringsFromFile(tmp, lines) then
    exit;
  pub := '';
  for i := 0 to GetArrayLength(lines) - 1 do
  begin
    low := Lowercase(lines[i]);
    if Pos('published name', low) > 0 then
      pub := Trim(Copy(lines[i], Pos(':', lines[i]) + 1, MaxInt));
    if (Pos('original name', low) > 0) and (Pos(OrigInfLower, low) > 0) and (pub <> '') then
    begin
      Exec(ExpandConstant('{sys}\pnputil.exe'), '/delete-driver ' + pub + ' /uninstall /force',
           '', SW_HIDE, ewWaitUntilTerminated, rc);
      pub := '';
    end;
  end;
  DeleteFile(tmp);
end;

{ ---- clean driver install: drop any existing devnode, purge every old store
       package for this INF, then install fresh -> exactly one devnode + one
       store copy, no matter how many times the installer is re-run. ----------- }
procedure InstallDriverPkg(Name, InfPath, Hwid, OrigInfLower: String);
var
  devcon: String;
  rc: Integer;
begin
  devcon := ExpandConstant('{app}\devcon\devcon.exe');
  Exec(devcon, 'remove ' + Hwid, '', SW_HIDE, ewWaitUntilTerminated, rc);
  DeleteStorePackage(OrigInfLower);
  if Exec(devcon, 'install "' + InfPath + '" ' + Hwid, '', SW_HIDE, ewWaitUntilTerminated, rc) then
  begin
    if rc = 1 then
      gNeedRestart := True
    else if rc <> 0 then
      MsgBox(Name + ': devcon could not create the device (exit code ' + IntToStr(rc) + ').' + #13#10 +
             'This usually means the driver catalog is not trusted on this machine' + #13#10 +
             '(install the signing certificate, or package a signed Release build).' + #13#10 + #13#10 +
             'Retry manually:  devcon install "' + InfPath + '" ' + Hwid, mbError, MB_OK);
  end
  else
    MsgBox(Name + ': failed to launch devcon.exe.', mbError, MB_OK);
end;

{ ---- Inno event hooks ------------------------------------------------------- }
function NeedRestart: Boolean;
begin
  Result := gNeedRestart;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
    if not (WizardIsComponentSelected('display') or WizardIsComponentSelected('input')) then
    begin
      MsgBox('Select at least one driver to install.', mbError, MB_OK);
      Result := False;
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if WizardIsComponentSelected('display') then
      InstallDriverPkg('VoidDisplay', ExpandConstant('{app}\display\driver\VoidDisplay.inf'), '{#HwidDisplay}', 'voiddisplay.inf');
    if WizardIsComponentSelected('input') then
      InstallDriverPkg('VoidInput', ExpandConstant('{app}\input\driver\VoidInput.inf'), '{#HwidInput}', 'voidinput.inf');
    if WizardIsTaskSelected('addtopath') then
      EnvAddPath(BinDir);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    { Remove whichever devnodes/packages exist - harmless if a driver was not installed. }
    RemoveDevnode('{#HwidDisplay}');
    RemoveDevnode('{#HwidInput}');
    DeleteStorePackage('voiddisplay.inf');
    DeleteStorePackage('voidinput.inf');
  end
  else if CurUninstallStep = usPostUninstall then
    EnvRemovePath(BinDir);
end;
