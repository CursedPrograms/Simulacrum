; Simulacrum installer (Inno Setup 6), built by build.bat from dist\Simulacrum.
; For you only (no administrator needed), into %LOCALAPPDATA%\Programs\Simulacrum - the folder the Atelier launcher
; uses too - with Start menu and Desktop shortcuts; everything it needs is inside, AliceVision included.

#define AppName "Simulacrum"
#define AppVersion "1.1.0"
#define AppPublisher "Cursed Entertainment"
#define AppURL "https://cursedprograms.github.io/Simulacrum/"

[Setup]
AppId={{308B2EC6-6095-4E43-93C9-CF43455F03BA}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL=https://github.com/CursedPrograms/Simulacrum/issues
AppUpdatesURL=https://github.com/CursedPrograms/Simulacrum/releases
DefaultDirName={localappdata}\Programs\Simulacrum
DefaultGroupName=Simulacrum
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
LicenseFile=..\LICENSE
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\Simulacrum.exe
UninstallDisplayName=Simulacrum (photos to 3D)
WizardStyle=modern
OutputDir=..\dist
OutputBaseFilename=Simulacrum-setup
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "A shortcut on the Desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\dist\Simulacrum\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Simulacrum"; Filename: "{app}\Simulacrum.exe"; Comment: "Photos to a textured 3D model"
Name: "{group}\Uninstall Simulacrum"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Simulacrum"; Filename: "{app}\Simulacrum.exe"; Comment: "Photos to a textured 3D model"; Tasks: desktopicon

[Run]
Filename: "{app}\Simulacrum.exe"; Description: "Start Simulacrum now"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=This puts Simulacrum on your PC: it turns photos of a thing, taken all the way round it, into a 3D model.%n%nEverything it needs comes with it. An NVIDIA graphics card makes the models far more detailed, but it works without one.
