#define AppName "ArZoom - Smart Camera Zoom & Follow for OBS"
#ifndef AppVersion
  #define AppVersion "0.3.1"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\arzoom-obs-v0.3.1-windows-x64"
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
DisableDirPage=yes
DirExistsWarning=no
UsePreviousAppDir=no
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
RestartApplications=no

[Files]
Source: "{#SourceDir}\obs-plugins\64bit\arzoom.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\obs-plugins\arzoom\*"; DestDir: "{app}\data\obs-plugins\arzoom"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; Flags: nowait postinstall skipifsilent; Check: FileExists(ExpandConstant('{app}\bin\64bit\obs64.exe'))

[UninstallDelete]
Type: filesandordirs; Name: "{app}\data\obs-plugins\arzoom"
Type: files; Name: "{app}\obs-plugins\64bit\arzoom.dll"

[Code]
var
  InstallModePage: TInputOptionWizardPage;
  PortableDirPage: TInputDirWizardPage;
  DetectedOBSRoot: String;

function NormalizeOBSRoot(Path: String): String;
begin
  Path := Trim(Path);
  if Path = '' then begin
    Result := '';
    Exit;
  end;
  Result := RemoveBackslashUnlessRoot(ExpandFileName(Path));
end;

function IsValidOBSRoot(Path: String): Boolean;
var
  Root: String;
begin
  Root := NormalizeOBSRoot(Path);
  Result := (Root <> '') and
            FileExists(AddBackslash(Root) + 'bin\64bit\obs64.exe');
end;

function TryRegistryOBSRoot(RootKey: Integer; KeyName: String): String;
var
  Candidate: String;
begin
  Result := '';
  if RegQueryStringValue(RootKey, KeyName, 'InstallLocation', Candidate) then begin
    Candidate := NormalizeOBSRoot(Candidate);
    if IsValidOBSRoot(Candidate) then
      Result := Candidate;
  end;
end;

function DetectOBSRoot(): String;
var
  Candidate: String;
begin
  Result := '';

  Candidate := TryRegistryOBSRoot(
    HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio');
  if Candidate <> '' then begin Result := Candidate; Exit; end;

  Candidate := TryRegistryOBSRoot(
    HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio_is1');
  if Candidate <> '' then begin Result := Candidate; Exit; end;

  Candidate := TryRegistryOBSRoot(
    HKCU64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio');
  if Candidate <> '' then begin Result := Candidate; Exit; end;

  Candidate := NormalizeOBSRoot(ExpandConstant('{autopf}\obs-studio'));
  if IsValidOBSRoot(Candidate) then begin Result := Candidate; Exit; end;

  Candidate := NormalizeOBSRoot(
    ExpandConstant('{localappdata}\Programs\obs-studio'));
  if IsValidOBSRoot(Candidate) then begin Result := Candidate; Exit; end;
end;

function SelectedOBSRoot(): String;
begin
  if (InstallModePage <> nil) and
     (InstallModePage.SelectedValueIndex = 0) then
    Result := DetectedOBSRoot
  else if (PortableDirPage <> nil) then
    Result := NormalizeOBSRoot(PortableDirPage.Values[0])
  else
    Result := '';
end;

procedure ApplySelectedOBSRoot();
var
  Root: String;
begin
  Root := SelectedOBSRoot();
  if Root <> '' then
    WizardForm.DirEdit.Text := Root;
end;

procedure InitializeWizard();
var
  StandardLabel: String;
  PreviousRoot: String;
begin
  DetectedOBSRoot := DetectOBSRoot();

  InstallModePage := CreateInputOptionPage(
    wpWelcome,
    'Choose your OBS installation',
    'ArZoom can install into normal OBS Studio or an OBS Portable/custom folder.',
    'Choose the type of OBS installation you want to update. The installer validates the selected folder before copying anything.',
    True,
    False);

  if DetectedOBSRoot <> '' then
    StandardLabel := 'Standard OBS Studio — auto detected: ' + DetectedOBSRoot
  else
    StandardLabel := 'Standard OBS Studio — auto detect';

  InstallModePage.Add(StandardLabel);
  InstallModePage.Add('OBS Portable / custom OBS folder');

  if DetectedOBSRoot <> '' then
    InstallModePage.SelectedValueIndex := 0
  else
    InstallModePage.SelectedValueIndex := 1;

  PortableDirPage := CreateInputDirPage(
    InstallModePage.ID,
    'Select the OBS Portable / custom root folder',
    'Choose the folder that contains your OBS installation.',
    'Select the OBS root folder. A valid folder contains bin\64bit\obs64.exe. Do not select the bin or obs-plugins subfolder.',
    False,
    '');
  PortableDirPage.Add('');

  PreviousRoot := GetPreviousData('LastOBSRoot', '');
  if IsValidOBSRoot(PreviousRoot) then
    PortableDirPage.Values[0] := PreviousRoot
  else if DetectedOBSRoot <> '' then
    PortableDirPage.Values[0] := DetectedOBSRoot
  else
    PortableDirPage.Values[0] := ExpandConstant('{userprofile}');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PortableDirPage <> nil) and
     (PageID = PortableDirPage.ID) then
    Result := InstallModePage.SelectedValueIndex <> 1;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Root: String;
begin
  Result := True;

  if (InstallModePage <> nil) and
     (CurPageID = InstallModePage.ID) and
     (InstallModePage.SelectedValueIndex = 0) and
     not IsValidOBSRoot(DetectedOBSRoot) then begin
    MsgBox(
      'A standard OBS Studio installation could not be detected.' + #13#10 + #13#10 +
      'Choose "OBS Portable / custom OBS folder" and select the OBS root folder manually.',
      mbInformation,
      MB_OK);
    InstallModePage.SelectedValueIndex := 1;
    Result := False;
    Exit;
  end;

  if (PortableDirPage <> nil) and
     (CurPageID = PortableDirPage.ID) then begin
    Root := NormalizeOBSRoot(PortableDirPage.Values[0]);
    if not IsValidOBSRoot(Root) then begin
      MsgBox(
        'This does not look like an OBS root folder:' + #13#10 +
        Root + #13#10 + #13#10 +
        'Please select the folder that contains:' + #13#10 +
        'bin\64bit\obs64.exe',
        mbError,
        MB_OK);
      Result := False;
      Exit;
    end;
  end;

  ApplySelectedOBSRoot();
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Root: String;
begin
  Result := '';
  Root := SelectedOBSRoot();
  if not IsValidOBSRoot(Root) then begin
    Result :=
      'ArZoom cannot install because the selected folder is not a valid OBS root folder.' + #13#10 +
      'Expected file: ' + AddBackslash(Root) + 'bin\64bit\obs64.exe';
    Exit;
  end;
  WizardForm.DirEdit.Text := Root;
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
var
  Root: String;
begin
  Root := SelectedOBSRoot();
  if IsValidOBSRoot(Root) then
    SetPreviousData(PreviousDataKey, 'LastOBSRoot', Root);
end;
