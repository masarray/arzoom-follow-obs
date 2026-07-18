#define AppName "ArZoom - Smart Mouse Zoom for OBS"
#ifndef AppVersion
  #define AppVersion "0.1.3"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\arzoom-obs-v0.1.3-windows-x64"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\release"
#endif

[Setup]
AppId={{9E1D217E-EA7D-4AF9-A86D-5BCEB0F03BC6}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Mas Ari
DefaultDirName={autopf}\obs-studio
DisableDirPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=ArZoom-OBS-Setup-v{#AppVersion}-windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
Uninstallable=yes
CloseApplications=yes

[Files]
Source: "{#SourceDir}\obs-plugins\64bit\arzoom.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\obs-plugins\arzoom\*"; DestDir: "{app}\data\obs-plugins\arzoom"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\data\obs-plugins\arzoom"
Type: files; Name: "{app}\obs-plugins\64bit\arzoom.dll"
