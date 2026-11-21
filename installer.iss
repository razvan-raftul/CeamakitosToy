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
CloseApplications=no

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
Source: "nvda\nvdaControllerClient.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "nvda\nvdaControllerClient.dll"; DestDir: "{commoncf64}\VST3\Ceamakitos toy.vst3\Contents\x86_64-win"; Flags: ignoreversion skipifsourcedoesntexist
Source: "reaper\*"; DestDir: "{userappdata}\REAPER\Scripts\Ceamakito's toy"; Flags: ignoreversion
Source: "instrumente\*"; DestDir: "{userdocs}\Ceamakito's toy\Instrumente"; Flags: recursesubdirs onlyifdoesntexist

[Icons]
Name: "{autodesktop}\Ceamakito's toy"; Filename: "{app}\Ceamakitos toy.exe"
Name: "{group}\Ceamakito's toy"; Filename: "{app}\Ceamakitos toy.exe"
Name: "{group}\Citeste-ma"; Filename: "{app}\CITESTE-MA.txt"

[Code]
function InitializeSetup(): Boolean;
var
  Code: Integer;
begin
  // la actualizarea singura, aplicatia apuca sa se inchida inainte sa fie inlocuita
  if WizardSilent then Sleep(3000);
  // daca aplicatia e inca deschisa, o inchid, ca sa poata fi inlocuita
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM "Ceamakitos toy.exe"', '', SW_HIDE, ewWaitUntilTerminated, Code);
  Sleep(500);
  Result := True;
end;
