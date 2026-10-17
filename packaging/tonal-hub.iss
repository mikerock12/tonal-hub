; Hub installer. Compile with build-hub-installer.ps1 after packaging the portable folder.
#ifndef SourceDir
  #error SourceDir is required
#endif
#ifndef AppVersion
  #define AppVersion "0.6.0.0"
#endif

[Setup]
AppId={{D47D88D5-B58A-49D2-868C-68A55BC3B4E2}
AppName=Tonal Hub
AppVersion={#AppVersion}
AppVerName=Tonal Hub {#AppVersion}
AppPublisher=Tonal Hub — projeto original de Matheus Nunes
AppPublisherURL=https://github.com/natheus/tonal-player
AppSupportURL=https://github.com/mikerock12/tonal-hub
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\Tonal Hub
DefaultGroupName=Tonal Hub
DisableProgramGroupPage=yes
AllowNoIcons=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=Tonal-Hub-{#AppVersion}-Windows-x64-Setup
SetupIconFile={#RepoRoot}\resources\tonal.ico
UninstallDisplayIcon={app}\TonalHub.exe
UninstallDisplayName=Tonal Hub
LicenseFile={#RepoRoot}\LICENSE
Compression=lzma2/normal
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoDescription=Tonal Hub — Tonalify e IndieTonal

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Tonal Hub"; Filename: "{app}\TonalHub.exe"; WorkingDir: "{app}"
Name: "{group}\Tonalify"; Filename: "{app}\Tonalify.exe"; WorkingDir: "{app}"; AppUserModelID: "Tonal.Player"
Name: "{group}\IndieTonal"; Filename: "{app}\IndieTonal.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Tonal Hub"; Filename: "{app}\TonalHub.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\TonalHub.exe"; Description: "{cm:LaunchProgram,Tonal Hub}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
