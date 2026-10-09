; Instalador do Tonal (Inno Setup 6). Rode pelo packaging\release.ps1, que passa AppVersion,
; SourceDir (pasta pronta do app), OutputDir e RepoRoot.
#ifndef AppVersion
  #error Rode pelo packaging\release.ps1
#endif

[Setup]
; AppId fixo: é o que faz uma versão nova atualizar a instalação anterior em vez de duplicar.
AppId={{66DAACCA-DA5C-4EB1-9BFE-AF5CDC44CEE4}
AppName=Tonal
AppVersion={#AppVersion}
AppVerName=Tonal {#AppVersion}
AppPublisherURL=https://github.com/natheus/tonal-player
AppSupportURL=https://github.com/natheus/tonal-player/issues
; Sem administrador por padrão: instala em %LOCALAPPDATA%\Programs\Tonal. O diálogo deixa
; escolher "para todos os usuários" (Program Files), que pede administrador.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={autopf}\Tonal
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Windows 10 1809 ou mais novo (mínimo do Qt 6.8)
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=Tonal-{#AppVersion}-setup
SetupIconFile={#RepoRoot}\resources\tonal.ico
UninstallDisplayIcon={app}\Tonal.exe
UninstallDisplayName=Tonal
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Fecha o Tonal aberto antes de atualizar ou desinstalar (o librespot fecha junto).
CloseApplications=yes

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Tonal"; Filename: "{app}\Tonal.exe"
Name: "{autodesktop}\Tonal"; Filename: "{app}\Tonal.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Tonal.exe"; Description: "{cm:LaunchProgram,Tonal}"; Flags: nowait postinstall skipifsilent
