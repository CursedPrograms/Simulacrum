; Terminus installer (Inno Setup 6), built by build.bat from dist\Terminus.
; For you only (no administrator needed), into %LOCALAPPDATA%\Programs\Terminus - the folder the Atelier launcher
; uses too - with Start menu and Desktop shortcuts; everything it needs is inside, AliceVision included.

#define AppName "Terminus"
#define AppVersion "1.0.0"
#define AppPublisher "Cursed Entertainment"
#define AppURL "https://cursedprograms.github.io/Terminus/"

[Setup]
AppId={{9B6E2D4C-7A1F-4E3B-8C5D-2F0A6B1E9D73}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL=https://github.com/CursedPrograms/Terminus/issues
AppUpdatesURL=https://github.com/CursedPrograms/Terminus/releases
DefaultDirName={localappdata}\Programs\Terminus
DefaultGroupName=Terminus
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
LicenseFile=..\LICENSE
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\Terminus.exe
UninstallDisplayName=Terminus (photos to 3D)
WizardStyle=modern
OutputDir=..\dist
OutputBaseFilename=Terminus-setup
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "A shortcut on the Desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\dist\Terminus\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Terminus"; Filename: "{app}\Terminus.exe"; Comment: "Photos to a textured 3D model"
Name: "{group}\Uninstall Terminus"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Terminus"; Filename: "{app}\Terminus.exe"; Comment: "Photos to a textured 3D model"; Tasks: desktopicon

[Run]
Filename: "{app}\Terminus.exe"; Description: "Start Terminus now"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=This puts Terminus on your PC: it turns photos of a thing, taken all the way round it, into a 3D model.%n%nEverything it needs comes with it. An NVIDIA graphics card makes the models far more detailed, but it works without one.
