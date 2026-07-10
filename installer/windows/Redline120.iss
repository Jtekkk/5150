; ============================================================================
;  Redline120.iss — Inno Setup script for the Windows installer.
;
;  Produces a signed-capable installer that drops the VST3 into the shared
;  VST3 folder and (optionally) the standalone app into Program Files, with a
;  proper uninstaller and Start-menu entry.
;
;  Prerequisites: build the plugin first (Release, x64), then run:
;      iscc installer\windows\Redline120.iss
;  The GitHub Actions workflow .github/workflows/windows-installer.yml does both
;  and uploads the resulting installer as a build artifact.
;
;  Inno Setup 6+ required (free): https://jrsoftware.org/isinfo.php
; ============================================================================

#define MyAppName      "Redline 120"
#define MyAppVersion   "0.1.0"
#define MyAppPublisher "TEKK Audio Labs"
#define MyAppURL       "https://github.com/Jtekkk/5150"
#define MyExeName      "Redline 120.exe"
#define MyVst3Name     "Redline 120.vst3"

; Directory holding the built artefacts. Override on the command line with
;   iscc /DArtefactsDir="C:\path\to\Redline120_artefacts\Release" ...
; Default matches a multi-config (Visual Studio) build from the repo root.
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\build\Redline120_artefacts\Release"
#endif

[Setup]
; A stable, unique AppId — do NOT change it between versions (it ties updates
; and the uninstaller together).
AppId={{9F3C7A21-5D84-4E6B-9C1A-2B7E6F0A3D55}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=Redline120-{#MyAppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Plugins install to Program Files / Common Files → needs elevation, 64-bit only.
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3";       Description: "VST3 plug-in (for your DAW)"; Types: full compact custom; Flags: checkablealone
Name: "standalone"; Description: "Standalone application";      Types: full custom;         Flags: checkablealone

[Files]
; VST3 is a bundle (folder) on Windows — install the whole tree into the shared
; VST3 location that every DAW scans.
Source: "{#ArtefactsDir}\VST3\{#MyVst3Name}\*"; DestDir: "{commonpf64}\VST3\{#MyVst3Name}"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone app + a copy of the docs.
Source: "{#ArtefactsDir}\Standalone\{#MyExeName}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\README.md";        DestDir: "{app}"; DestName: "README.txt";     Components: standalone; Flags: ignoreversion isreadme
Source: "..\..\docs\DESIGN.md";   DestDir: "{app}"; DestName: "DESIGN.txt";     Components: standalone; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}";            Filename: "{app}\{#MyExeName}"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}";  Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyExeName}"; Description: "Launch {#MyAppName}"; \
    Components: standalone; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Remove the VST3 bundle folder we created (its files are removed automatically,
; this cleans up the now-empty directory).
Type: filesandordirs; Name: "{commonpf64}\VST3\{#MyVst3Name}"

[Code]
// Refuse to run if neither the VST3 nor the standalone artefact is present —
// gives a clear error instead of a broken install if the build step was skipped.
function InitializeSetup(): Boolean;
begin
  Result := True;
  if not (DirExists(ExpandConstant('{#ArtefactsDir}\VST3\{#MyVst3Name}'))
       or FileExists(ExpandConstant('{#ArtefactsDir}\Standalone\{#MyExeName}'))) then
  begin
    MsgBox('Build artefacts were not found in:' + #13#10 +
           ExpandConstant('{#ArtefactsDir}') + #13#10#13#10 +
           'Build the plugin (Release, x64) before running this installer.',
           mbCriticalError, MB_OK);
    Result := False;
  end;
end;
