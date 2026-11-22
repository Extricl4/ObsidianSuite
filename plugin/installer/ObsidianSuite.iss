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
; /DBuild=145: the GitHub build number. /DUpdateOnly=1: the small installer the app's UPDATE button
; downloads: everything except the big capture / IR pack (it leaves yours as it is).
#ifndef Build
  #define Build "0"
#endif

[Setup]
AppId={{6A3F1C2E-5B7D-4E8A-9C1F-0B5D0B5D0B5D}
AppName=Obsidian Amp Suite
AppVersion={#AppVersion}
AppVerName=Obsidian Amp Suite {#AppVersion} (build {#Build})
AppPublisher=Emil
AppPublisherURL=https://github.com/Extricl4/ObsidianSuite
DefaultDirName={autopf}\Obsidian Amp Suite
DefaultGroupName=Obsidian Amp Suite
DisableProgramGroupPage=yes
UsePreviousAppDir=no
UsePreviousGroup=no
#ifdef UpdateOnly
OutputBaseFilename=ObsidianAmpSuite-{#AppVersion}-Update
#else
OutputBaseFilename=ObsidianAmpSuite-{#AppVersion}-Setup
#endif
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Obsidian Amp Suite
; a DAW that has the plugin open holds the .vst3: list it on "Preparing to install" and offer to close it
CloseApplicationsFilter=*.exe,*.dll,*.chm,*.vst3

[InstallDelete]
; the first test build was called "Obsidian Suite": remove it so the DAW doesn't list two plugins
Type: filesandordirs; Name: "{commoncf64}\VST3\Obsidian Suite.vst3"
Type: filesandordirs; Name: "{autopf}\Obsidian Suite"
Type: filesandordirs; Name: "{autoprograms}\Obsidian Suite"
Type: files; Name: "{autodesktop}\Obsidian Suite.lnk"

[Components]
Name: "vst3"; Description: "VST3 plugin (for your DAW)"; Types: full compact custom; Flags: fixed
Name: "app"; Description: "Standalone app (play without a DAW, ASIO)"; Types: full custom
Name: "pack"; Description: "Pedal and amp pack (captures, reverb IRs, the 30 capture amps and their faces)"; Types: full custom

[Files]
Source: "{#SourceDir}\Obsidian Amp Suite.vst3\*"; DestDir: "{commoncf64}\VST3\Obsidian Amp Suite.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceDir}\Obsidian Amp Suite.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion
; the amp pack's amp files and faces are ours: always updated
Source: "{#SourceDir}\Content\Amps\*"; DestDir: "{userdocs}\Obsidian Amp Suite\Amps"; Components: pack; Flags: ignoreversion uninsneveruninstall
Source: "{#SourceDir}\Content\Faces\*"; DestDir: "{userdocs}\Obsidian Amp Suite\Faces"; Components: pack; Flags: recursesubdirs createallsubdirs ignoreversion uninsneveruninstall
#ifndef UpdateOnly
; pedal + amp pack -> Documents\Obsidian Amp Suite (Captures\..., IRs\...); never overwrites your files
Source: "{#SourceDir}\Content\*"; DestDir: "{userdocs}\Obsidian Amp Suite"; Components: pack; Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
#endif

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
; the app's UPDATE button installs quietly and the app closes: open it again when done (not as admin)
Filename: "{app}\Obsidian Amp Suite.exe"; Components: app; Flags: nowait runasoriginaluser; Check: WizardSilent
