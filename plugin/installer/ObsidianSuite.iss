; Obsidian Suite installer (Inno Setup 6). Built by the GitHub workflow:
;   iscc /DAppVersion=0.2.0 /DSourceDir=<folder with the .vst3 and .exe> ObsidianSuite.iss
; Installs the VST3 where every DAW looks for it, the standalone app with a
; Start-menu shortcut, and creates Documents\Obsidian Suite for your captures,
; IRs and presets.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\ObsidianSuite"
#endif

[Setup]
AppId={{6A3F1C2E-5B7D-4E8A-9C1F-0B5D0B5D0B5D}
AppName=Obsidian Suite
AppVersion={#AppVersion}
AppPublisher=Emil
AppPublisherURL=https://github.com/Extricl4/ObsidianSuite
DefaultDirName={autopf}\Obsidian Suite
DefaultGroupName=Obsidian Suite
DisableProgramGroupPage=yes
OutputBaseFilename=ObsidianSuite-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Obsidian Suite

[Components]
Name: "vst3"; Description: "VST3 plugin (for your DAW)"; Types: full compact custom; Flags: fixed
Name: "app"; Description: "Standalone app (play without a DAW, ASIO)"; Types: full custom

[Files]
Source: "{#SourceDir}\Obsidian Suite.vst3\*"; DestDir: "{commoncf64}\VST3\Obsidian Suite.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceDir}\Obsidian Suite.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{userdocs}\Obsidian Suite\Presets"
Name: "{userdocs}\Obsidian Suite\Captures"
Name: "{userdocs}\Obsidian Suite\IRs"
Name: "{userdocs}\Obsidian Suite\Amps"
Name: "{userdocs}\Obsidian Suite\Cabs"

[Icons]
Name: "{group}\Obsidian Suite"; Filename: "{app}\Obsidian Suite.exe"; Components: app
Name: "{autodesktop}\Obsidian Suite"; Filename: "{app}\Obsidian Suite.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the standalone app"; Components: app

[Run]
Filename: "{app}\Obsidian Suite.exe"; Description: "Start Obsidian Suite"; Components: app; Flags: nowait postinstall skipifsilent
