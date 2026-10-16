; Mz808 - an emulation of the TR-808 Rhythm Composer
; Copyright (C) 2026 Simon Moscrop
;
; This program is free software: you can redistribute it and/or modify it under the terms of
; the GNU Affero General Public License as published by the Free Software Foundation, version 3.
; It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

;
; The Windows installer: the VST3 and the standalone in one Setup.exe.
;
;     .\build.ps1 -Release         builds the plugin, then this, into dist\
;
; build.ps1 passes the version and copyright read from CMakeLists.txt, so each is written down
; once. Compiled by hand without them, the installer says 0.0.0 rather than a version it was
; not built from.
;
; Setup asks whether to install for everyone or just this user. Everyone (the default) puts
; the VST3 in C:\Program Files\Common Files\VST3, which every DAW scans; just me needs no
; administrator rights and uses %LOCALAPPDATA%\Programs\Common\VST3, the folder build.ps1
; installs to, which some hosts need adding to their plugin paths.
;
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppCopyright
  #define AppCopyright "Copyright (C) Simon Moscrop"
#endif

#define Artefacts "..\build\Mz808_artefacts\Release"

[Setup]
; The installer's identity, as the plugin codes are the plugin's: never change it, or a new
; version installs beside the old one rather than upgrading it.
AppId={{B332831B-F0C7-4F35-AFB7-122E3B7039A9}
AppName=Mz808
AppVersion={#AppVersion}
AppVerName=Mz808 {#AppVersion}
AppPublisher=simozzer
AppPublisherURL=https://github.com/simozzer/Mz808
AppSupportURL=https://github.com/simozzer/Mz808/issues
AppUpdatesURL=https://github.com/simozzer/Mz808/releases
AppCopyright={#AppCopyright}. Free software under the GNU AGPL v3.
VersionInfoCopyright={#AppCopyright}
VersionInfoVersion={#AppVersion}

DefaultDirName={autopf}\Mz808
DefaultGroupName=Mz808
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

OutputDir=..\dist
OutputBaseFilename=Mz808-{#AppVersion}-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Mz808.exe
UninstallDisplayName=Mz808 {#AppVersion}

; A DAW with the plugin loaded holds Mz808.vst3 open; let Setup offer to close it.
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full vst3 custom
Name: "standalone"; Description: "Standalone app (plays without a DAW)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[Files]
; {autocf} is Common Files for everyone and %LOCALAPPDATA%\Programs\Common for just me.
Source: "{#Artefacts}\VST3\Mz808.vst3\*"; DestDir: "{autocf}\VST3\Mz808.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Mz808.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Mz808"; Filename: "{app}\Mz808.exe"; Components: standalone
Name: "{autodesktop}\Mz808"; Filename: "{app}\Mz808.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Mz808.exe"; Description: "{cm:LaunchProgram,Mz808}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
