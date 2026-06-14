; ===========================================================================
;  VoidDisplay.iss - Inno Setup 7 installer for the Void Virtual Display Adapter
; ===========================================================================
;
;  One installer that targets BOTH x64 and ARM64 Windows. It:
;    * detects the OS architecture and lays down the matching driver package,
;    * creates the root-enumerated device node with the bundled devcon.exe,
;    * creates the operator config folder C:\ProgramData\.voidrv (writable by any
;      logged-in user so an unelevated voidctl can persist display.ini, no UAC),
;    * installs voidctl into C:\ProgramData\.voidrv\bin (optionally on PATH),
;    * runs elevated (admin).
;
;  Build:
;    "C:\Program Files\Inno Setup 7\ISCC.exe" installer\VoidDisplay.iss
;  Override which build flavor is packaged (default Debug, what bring-up produces):
;    ... /DDrvConfig=Release /DCtlConfig=Release
;
;  NOTE on signing: devcon installs the driver only if its catalog (.cat) is trusted
;  on the target machine - the dev test certificate in Root + TrustedPublisher, or an
;  OV/EV Authenticode signature for release. This script does not manage certificates.
; ===========================================================================

#define AppName       "VoidDisplay"
#define AppPublisher  "Void Virtual Driver"
#define AppVersion    "1.0.0"
#define HardwareId    "Root\Void\Display"

; Which build configuration of each component to package. Driver builds both Debug and
; Release for x64+ARM64; voidctl is x64-only today. Override on the ISCC command line.
#ifndef DrvConfig
  #define DrvConfig "Debug"
#endif
#ifndef CtlConfig
  #define CtlConfig "Debug"
#endif

; Source trees, relative to this .iss (which lives in installer\).
#define DrvX64    "..\void-display\x64\"   + DrvConfig + "\VoidDisplay"
#define DrvArm64  "..\void-display\ARM64\" + DrvConfig + "\VoidDisplay"
#define CtlX64    "..\voidctl\x64\"         + CtlConfig + "\voidctl.exe"

[Setup]
AppId={{3E540C70-0E6D-405C-9860-6A58983FB174}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Void\{#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
; x64 and ARM64 only - there is no 32-bit x86 driver, so refuse on plain x86.
ArchitecturesAllowed=x64compatible or arm64
ArchitecturesInstallIn64BitMode=x64compatible or arm64
Compression=lzma2/max
WizardStyle=modern
ChangesEnvironment=yes
UninstallDisplayName={#AppName} (Void Virtual Display Adapter)
OutputDir=out
OutputBaseFilename=VoidDisplay-{#AppVersion}-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "addtopath"; Description: "Add voidctl to PATH (%ProgramData%\.voidrv\bin)"; GroupDescription: "Command-line tools:"

[Dirs]
; The config folder must be writable by any logged-in user so an UNELEVATED voidctl can
; persist display.ini with no UAC prompt (the driver only READS it, running as SYSTEM).
; The bin subfolder below is re-secured read-only in [Code] (HardenBinDir) so the
; voidctl.exe we put on PATH cannot be replaced by a non-admin.
Name: "{commonappdata}\.voidrv"; Permissions: everyone-modify

[Files]
; ---- Driver package: only the OS-native architecture is actually installed ----
Source: "{#DrvArm64}\VoidDisplay.dll"; DestDir: "{app}\driver"; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DrvArm64}\VoidDisplay.inf"; DestDir: "{app}\driver"; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DrvArm64}\voiddisplay.cat"; DestDir: "{app}\driver"; Check: PreferArm64Files; Flags: ignoreversion
Source: "{#DrvX64}\VoidDisplay.dll";   DestDir: "{app}\driver"; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#DrvX64}\VoidDisplay.inf";   DestDir: "{app}\driver"; Check: PreferX64Files;   Flags: ignoreversion
Source: "{#DrvX64}\voiddisplay.cat";   DestDir: "{app}\driver"; Check: PreferX64Files;   Flags: ignoreversion

; ---- devcon.exe: OS-native arch (flattened names dodge the repo's x64/ ARM64/ ignores) ----
Source: "redist\devcon\devcon-arm64.exe"; DestDir: "{app}\devcon"; DestName: "devcon.exe"; Check: PreferArm64Files; Flags: ignoreversion
Source: "redist\devcon\devcon-x64.exe";   DestDir: "{app}\devcon"; DestName: "devcon.exe"; Check: PreferX64Files;   Flags: ignoreversion

; ---- voidctl CLI -> ProgramData\.voidrv\bin (x64 binary; native on x64, x64-emulated on ARM64) ----
Source: "{#CtlX64}"; DestDir: "{commonappdata}\.voidrv\bin"; Flags: ignoreversion

; ---- default config: seeded only if absent, and preserved across upgrade/uninstall ----
Source: "config\display.ini"; DestDir: "{commonappdata}\.voidrv"; Flags: onlyifdoesntexist uninsneveruninstall

[Code]
const
  HWID    = '{#HardwareId}';
  EnvKey  = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

var
  gNeedRestart: Boolean;

{ ---- architecture selection (mirrors Inno's three-arch example) ------------- }
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
  Result := ExpandConstant('{commonappdata}\.voidrv\bin');
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

{ ---- re-secure the bin subfolder (it sits under a world-writable parent) ----- }
procedure HardenBinDir;
var
  rc: Integer;
begin
  { Break inheritance, then: Users = read/execute, Administrators + SYSTEM = full.
    Keeps voidctl.exe out of a non-admin's reach even though .voidrv itself is
    everyone-writable for display.ini. }
  Exec(ExpandConstant('{sys}\icacls.exe'),
       '"' + BinDir + '" /inheritance:r' +
       ' /grant:r "*S-1-5-32-545:(OI)(CI)(RX)"' +
       ' /grant:r "*S-1-5-32-544:(OI)(CI)(F)"' +
       ' /grant:r "*S-1-5-18:(OI)(CI)(F)"',
       '', SW_HIDE, ewWaitUntilTerminated, rc);
end;

{ ---- driver install via devcon (creates the root devnode + stages to store) -- }
procedure InstallDriver;
var
  devcon, inf: String;
  rc: Integer;
begin
  devcon := ExpandConstant('{app}\devcon\devcon.exe');
  inf    := ExpandConstant('{app}\driver\VoidDisplay.inf');
  { Remove any prior devnode first so re-running the installer never stacks duplicates. }
  Exec(devcon, 'remove ' + HWID, '', SW_HIDE, ewWaitUntilTerminated, rc);
  if Exec(devcon, 'install "' + inf + '" ' + HWID, '', SW_HIDE, ewWaitUntilTerminated, rc) then
  begin
    if rc = 1 then
      gNeedRestart := True
    else if rc <> 0 then
      MsgBox('VoidDisplay: devcon could not create the device (exit code ' + IntToStr(rc) + ').' + #13#10 +
             'This usually means the driver catalog is not trusted on this machine' + #13#10 +
             '(install the signing certificate, or package a signed Release build).' + #13#10 + #13#10 +
             'Retry manually:  devcon install "' + inf + '" ' + HWID, mbError, MB_OK);
  end
  else
    MsgBox('VoidDisplay: failed to launch devcon.exe.', mbError, MB_OK);
end;

{ ---- uninstall: drop the devnode, then delete the staged store package ------- }
procedure DeleteDriverPackageFromStore;
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
    if (Pos('original name', low) > 0) and (Pos('voiddisplay.inf', low) > 0) and (pub <> '') then
    begin
      Exec(ExpandConstant('{sys}\pnputil.exe'), '/delete-driver ' + pub + ' /uninstall /force',
           '', SW_HIDE, ewWaitUntilTerminated, rc);
      pub := '';
    end;
  end;
  DeleteFile(tmp);
end;

procedure RemoveDriver;
var
  devcon: String;
  rc: Integer;
begin
  devcon := ExpandConstant('{app}\devcon\devcon.exe');
  if FileExists(devcon) then
    Exec(devcon, 'remove ' + HWID, '', SW_HIDE, ewWaitUntilTerminated, rc);
  DeleteDriverPackageFromStore;
end;

{ ---- Inno event hooks ------------------------------------------------------- }
function NeedRestart: Boolean;
begin
  Result := gNeedRestart;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    HardenBinDir;
    InstallDriver;
    if WizardIsTaskSelected('addtopath') then
      EnvAddPath(BinDir);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveDriver
  else if CurUninstallStep = usPostUninstall then
    EnvRemovePath(BinDir);
end;
