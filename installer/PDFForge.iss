; =====================================================================
;  PDFForge installer - build with Inno Setup 6 (free, jrsoftware.org)
;
;     iscc installer\PDFForge.iss
;
;  produces  output\PDFForgeSetup-1.0.0.exe  (Start Menu entry, Add or
;  Remove Programs entry, optional desktop icon and .pdf "Open with").
;
;  NOTE: PDFForge itself needs NO installation - PDFForge.exe is fully
;  portable.  This installer only exists for people who prefer the
;  classic Start Menu / uninstaller experience.
;
;  Author : Ragul S
; =====================================================================

#define MyAppName      "PDFForge"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "Ragul S"
#define MyAppExeName   "PDFForge.exe"

[Setup]
AppId={{6F2A9C41-3B7D-4E8A-9C15-D0FF09E00001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppComments=Every PDF tool you need, in one offline app.
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=output
OutputBaseFilename=PDFForgeSetup-{#MyAppVersion}
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
Name: "assocpdf";   Description: "Add PDFForge to the &""Open with"" list for PDF files"; GroupDescription: "File associations:"; Flags: unchecked

[Files]
Source: "..\bin\PDFForge.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md";        DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY.md";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";             Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}";   Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";       Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; "Open with PDFForge" - registers a verb WITHOUT stealing the default handler
Root: HKCR; Subkey: "PDFForge.PDF";                    ValueType: string; ValueData: "PDF document";            Flags: uninsdeletekey
Root: HKCR; Subkey: "PDFForge.PDF\DefaultIcon";        ValueType: string; ValueData: "{app}\{#MyAppExeName},0";  Flags: uninsdeletekey
Root: HKCR; Subkey: "PDFForge.PDF\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey
Root: HKCR; Subkey: ".pdf\OpenWithProgIds";            ValueType: string; ValueName: "PDFForge.PDF"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocpdf

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch PDFForge"; Flags: nowait postinit skipifsilent
