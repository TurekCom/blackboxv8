#define AppName "BlackBox V8 SAPI5"
#define AppVersion "0.5.13"
#define EngineDllName64 "BlackBoxSapi5.dll"
#define ConfigExeName "BlackBoxSapi5Config.exe"
#define VoiceToken "BlackBoxV8.Sapi5"
#define VoiceClsid "{{9B5D8344-4D5E-46A2-80D5-2D83CC6BC27D}"

[Setup]
AppId={{A9DAA35E-7E73-4F4A-8730-6E8C7754B22F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=BlackBox Project
DefaultDirName={autopf}\BlackBox SAPI5
DefaultGroupName={#AppName}
OutputDir=..\dist\installer
OutputBaseFilename=BlackBoxSapi5-{#AppVersion}-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"

[Files]
Source: "..\sapi5\build-x64\Release\{#EngineDllName64}"; DestDir: "{app}\x64"; Flags: ignoreversion
Source: "..\sapi5\build-x64\Release\{#ConfigExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\android\app\src\main\assets\emoji\emoji_pl_cldr.tsv"; DestDir: "{app}\x64"; Flags: ignoreversion

[Icons]
Name: "{group}\Ustawienia BlackBox V8"; Filename: "{app}\{#ConfigExeName}"
Name: "{commondesktop}\Ustawienia BlackBox V8"; Filename: "{app}\{#ConfigExeName}"

[Run]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\x64\{#EngineDllName64}"""; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\x64\{#EngineDllName64}"""; Flags: runhidden waituntilterminated

[Registry]
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: ""; ValueData: "BlackBox V8 (SAPI5 x64)"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "CLSID"; ValueData: "{#VoiceClsid}"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "409"; ValueData: "BlackBox V8"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}\Attributes"; ValueType: string; ValueName: "Name"; ValueData: "BlackBox V8"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}\Attributes"; ValueType: string; ValueName: "Gender"; ValueData: "Male"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}\Attributes"; ValueType: string; ValueName: "Age"; ValueData: "Adult"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}\Attributes"; ValueType: string; ValueName: "Language"; ValueData: "0415"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}\Attributes"; ValueType: string; ValueName: "Vendor"; ValueData: "BlackBox"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "IntonationMode"; ValueData: "auto"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: dword; ValueName: "IntonationStrength"; ValueData: "110"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "SymbolLevel"; ValueData: "most"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "NumberMode"; ValueData: "cardinal"
Root: HKLM64; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\{#VoiceToken}"; ValueType: string; ValueName: "VoiceFlavor"; ValueData: "c64"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RegDeleteKeyIncludingSubkeys(HKLM64, 'SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\{#VoiceToken}');
  end;
end;
