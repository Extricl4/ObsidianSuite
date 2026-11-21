; Obsidian Amp Suite installer (Inno Setup 6). Built by the GitHub workflow:
;   iscc /DAppVersion=0.2.0 /DSourceDir=<folder with the .vst3 and .exe> ObsidianSuite.iss
; Installs the VST3 where every DAW looks for it, the standalone app with a
; Start-menu shortcut, and creates Documents\Obsidian Amp Suite for your captures,
; IRs and presets.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\ObsidianSuite"
#endif

[Setup]
AppId={{6A3F1C2E-5B7D-4E8A-9C1F-0B5D0B5D0B5D}
AppName=Obsidian Amp Suite
AppVersion={#AppVersion}
AppPublisher=Emil
AppPublisherURL=https://github.com/Extricl4/ObsidianSuite
DefaultDirName={autopf}\Obsidian Amp Suite
DefaultGroupName=Obsidian Amp Suite
DisableProgramGroupPage=yes
UsePreviousAppDir=no
UsePreviousGroup=no
OutputBaseFilename=ObsidianAmpSuite-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Obsidian Amp Suite

[InstallDelete]
; the first test build was called "Obsidian Suite": remove it so the DAW doesn't list two plugins
Type: filesandordirs; Name: "{commoncf64}\VST3\Obsidian Suite.vst3"
Type: filesandordirs; Name: "{autopf}\Obsidian Suite"
Type: filesandordirs; Name: "{autoprograms}\Obsidian Suite"
Type: files; Name: "{autodesktop}\Obsidian Suite.lnk"

[Components]
Name: "vst3"; Description: "VST3 plugin (for your DAW)"; Types: full compact custom; Flags: fixed
Name: "app"; Description: "Standalone app (play without a DAW, ASIO)"; Types: full custom

[Files]
Source: "{#SourceDir}\Obsidian Amp Suite.vst3\*"; DestDir: "{commoncf64}\VST3\Obsidian Amp Suite.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceDir}\Obsidian Amp Suite.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{userdocs}\Obsidian Amp Suite\Presets"
Name: "{userdocs}\Obsidian Amp Suite\Captures"
Name: "{userdocs}\Obsidian Amp Suite\IRs"
Name: "{userdocs}\Obsidian Amp Suite\Amps"
Name: "{userdocs}\Obsidian Amp Suite\Cabs"

[Icons]
Name: "{group}\Obsidian Amp Suite"; Filename: "{app}\Obsidian Amp Suite.exe"; Components: app
Name: "{autodesktop}\Obsidian Amp Suite"; Filename: "{app}\Obsidian Amp Suite.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the standalone app"; Components: app

[Run]
Filename: "{app}\Obsidian Amp Suite.exe"; Description: "Start Obsidian Amp Suite"; Components: app; Flags: nowait postinstall skipifsilent
