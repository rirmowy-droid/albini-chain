; Albini Chain - instalator Windows (Inno Setup 6)
#define AppName "Albini Chain"
#define AppVer GetEnv("APPVER")
#if AppVer == ""
  #define AppVer "1.1.0"
#endif
#define Vst3Src "..\build\AlbiniChain_artefacts\Release\VST3\Albini Chain.vst3"

[Setup]
AppId={{7C035E40-6987-42CA-AB21-FDAB78348D6C}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=rirmowy-droid
DefaultDirName={commoncf64}\VST3
DirExistsWarning=no
DisableDirPage=yes
DisableProgramGroupPage=yes
CreateUninstallRegKey=yes
UninstallDisplayName={#AppName} (VST3)
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

[Files]
Source: "{#Vst3Src}\*"; DestDir: "{commoncf64}\VST3\Albini Chain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Albini Chain.vst3"

[Messages]
polish.FinishedLabel=Wtyczka Albini Chain została zainstalowana w folderze VST3. W programie muzycznym uruchom ponowne skanowanie wtyczek.
