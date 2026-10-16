; Moon Engine Windows installer (Inno Setup 6).
; Build the release folder first with tools/package_release.py, then:
;   ISCC /DVersion=0.2.0-alpha tools\installer\moonengine.iss
; The installer holds only what that folder holds: no game data.

#ifndef Version
  #error Pass /DVersion=<version>
#endif
#define Source "..\..\dist\MoonEngine-" + Version + "-win64"

[Setup]
AppId={{6B0E5C2A-4F1D-4B8E-9A47-0D3C5E2F7A91}
AppName=Moon Engine
AppVersion={#Version}
AppVerName=Moon Engine {#Version}
AppPublisher=Moon Engine contributors
AppComments=Unofficial native PC port of Gran Turismo 4. Requires your own game disc image.
DefaultDirName={autopf}\MoonEngine
DefaultGroupName=Moon Engine
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#Source}\LICENSE.txt
InfoBeforeFile={#Source}\NOTICE.txt
OutputDir=..\..\dist
OutputBaseFilename=MoonEngine-{#Version}-setup
UninstallDisplayIcon={app}\MoonEngine.exe
SetupIconFile=..\..\res\moonengine.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Source}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MoonEngine"; Filename: "{app}\MoonEngine.exe"
Name: "{autoprograms}\MoonEngine README"; Filename: "{app}\README.txt"
Name: "{autodesktop}\MoonEngine"; Filename: "{app}\MoonEngine.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MoonEngine.exe"; Description: "{cm:LaunchProgram,Moon Engine}"; Flags: nowait postinstall skipifsilent

; Updater invokes /VERYSILENT /CURRENTUSER. Saves live outside {app}.
Filename: "{app}\MoonEngine.exe"; Flags: nowait; Check: WizardSilent
