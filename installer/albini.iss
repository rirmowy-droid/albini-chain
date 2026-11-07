; Albini Chain + Stonoga x Albini - instalator Windows (Inno Setup 6)
#define AppName "Albini Chain"
#define AppVer GetEnv("APPVER")
#if AppVer == ""
  #define AppVer "2.0.0"
#endif
#define Art "..\build"
#define AlbiniVst3 Art + "\AlbiniChain_artefacts\Release\VST3\Albini Chain.vst3"
#define ComboVst3  Art + "\StonogaAlbini_artefacts\Release\VST3\Stonoga x Albini.vst3"
#define ComboExe   Art + "\StonogaAlbini_artefacts\Release\Standalone\Stonoga x Albini.exe"

[Setup]
AppId={{7C035E40-6987-42CA-AB21-FDAB78348D6C}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=rirmowy-droid
DefaultDirName={autopf}\Albini Chain
DisableDirPage=yes
UsePreviousAppDir=no
DisableProgramGroupPage=yes
UninstallDisplayName={#AppName} + Stonoga x Albini
UninstallDisplayIcon={app}\Stonoga x Albini.exe
OutputDir=..\dist
OutputBaseFilename=AlbiniChain-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Skrót na pulpicie do Stonoga x Albini (bez DAW)"; Flags: unchecked

[Files]
Source: "{#AlbiniVst3}\*"; DestDir: "{commoncf64}\VST3\Albini Chain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ComboVst3}\*";  DestDir: "{commoncf64}\VST3\Stonoga x Albini.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ComboExe}";     DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Stonoga x Albini (bez DAW)"; Filename: "{app}\Stonoga x Albini.exe"
Name: "{autodesktop}\Stonoga x Albini"; Filename: "{app}\Stonoga x Albini.exe"; Tasks: desktopicon

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Albini Chain.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Stonoga x Albini.vst3"

[Messages]
polish.FinishedLabel=Zainstalowano wtyczki Albini Chain i Stonoga x Albini (VST3) oraz aplikację Stonoga x Albini (bez DAW) w menu Start. W programie muzycznym uruchom ponowne skanowanie wtyczek.
