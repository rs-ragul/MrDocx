; =====================================================================
;  MrDocx installer - build with Inno Setup 6 (free, jrsoftware.org)
;
;     iscc installer\MrDocx.iss
;
;  produces  output\MrDocxSetup-1.0.0.exe  (Start Menu entry, Add or
;  Remove Programs entry, optional desktop icon and .pdf "Open with").
;
;  NOTE: MrDocx itself needs NO installation - MrDocx.exe is fully
;  portable.  This installer only exists for people who prefer the
;  classic Start Menu / uninstaller experience.
;
;  Author : Ragul S
; =====================================================================

#define MyAppName      "MrDocx"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "Ragul S"
#define MyAppExeName   "MrDocx.exe"

[Setup]
AppId={{6F2A9C41-3B7D-4E8A-9C15-D0FF09E00001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppComments=Every PDF tool you need, in one offline app.
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=output
OutputBaseFilename=MrDocxSetup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
LicenseFile=..\third_party\mupdf\LICENSE.mupdf.AGPL-3.0.txt
WizardStyle=modern
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"
Name: "assocpdf";   Description: "Add MrDocx to the &""Open with"" list for PDF files"; GroupDescription: "File associations:"; Flags: unchecked

[Files]
Source: "..\bin\MrDocx.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md";        DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY.md";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";             Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}";   Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";       Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; "Open with MrDocx" - registers a verb WITHOUT stealing the default handler
Root: HKCR; Subkey: "MrDocx.PDF";                    ValueType: string; ValueData: "PDF document";            Flags: uninsdeletekey
Root: HKCR; Subkey: "MrDocx.PDF\DefaultIcon";        ValueType: string; ValueData: "{app}\{#MyAppExeName},0";  Flags: uninsdeletekey
Root: HKCR; Subkey: "MrDocx.PDF\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey
Root: HKCR; Subkey: ".pdf\OpenWithProgIds";            ValueType: string; ValueName: "MrDocx.PDF"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocpdf

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch MrDocx"; Flags: nowait postinit skipifsilent
