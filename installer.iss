#define Ver GetEnv("VER")
#if Ver == ""
  #define Ver "0.1.0"
#endif

[Setup]
AppId={{6C2B7E52-3A41-4E0B-9F2D-CE4A6B1F7A11}
AppName=Ceamakito's toy
AppVersion={#Ver}
AppPublisher=Razvan
DefaultDirName={autopf}\Ceamakito's toy
DefaultGroupName=Ceamakito's toy
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=out
OutputBaseFilename=Instaleaza Ceamakitos toy
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UsedUserAreasWarning=no
WizardStyle=modern
UninstallDisplayName=Ceamakito's toy

[Languages]
#if FileExists(AddBackslash(CompilerPath) + "Languages\Romanian.isl")
Name: "ro"; MessagesFile: "compiler:Languages\Romanian.isl"
#else
Name: "en"; MessagesFile: "compiler:Default.isl"
#endif

[InstallDelete]
Type: filesandordirs; Name: "{localappdata}\Programs\Common\VST3\Ceamakitos toy.vst3"

[Files]
Source: "build\CeamakitosToy_artefacts\Release\VST3\Ceamakitos toy.vst3\*"; DestDir: "{commoncf64}\VST3\Ceamakitos toy.vst3"; Flags: recursesubdirs ignoreversion
Source: "build\CeamakitosToy_artefacts\Release\Standalone\Ceamakitos toy.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "CITESTE-MA.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "reaper\*"; DestDir: "{userappdata}\REAPER\Scripts\Ceamakito's toy"; Flags: ignoreversion
Source: "instrumente\*"; DestDir: "{userdocs}\Ceamakito's toy\Instrumente"; Flags: recursesubdirs onlyifdoesntexist

[Icons]
Name: "{autodesktop}\Ceamakito's toy"; Filename: "{app}\Ceamakitos toy.exe"
Name: "{group}\Ceamakito's toy"; Filename: "{app}\Ceamakitos toy.exe"
Name: "{group}\Citeste-ma"; Filename: "{app}\CITESTE-MA.txt"

[Run]
Filename: "{app}\Ceamakitos toy.exe"; Description: "Deschide Ceamakito's toy"; Flags: nowait postinstall skipifsilent
