; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{289FCFCA-1001-4BD7-BA62-6B58CC38D8D8}
AppName=中国国际期货网上交易终端(模拟版-plugins)
AppVerName=中国国际期货网上交易终端(模拟版-plugins) 1.7.2.30
AppPublisher=CIFCO IT SERVICES CO.,LTD
AppPublisherURL=http://www.cifco.net
AppSupportURL=http://www.cifco.net
AppUpdatesURL=http://www.cifco.net
DefaultDirName={pf32}\中国国际期货网上交易终端(模拟版-plugins)
DisableDirPage=no
DefaultGroupName=中国国际期货网上交易终端(模拟版-plugins)
OutputDir=..\output
OutputBaseFilename=FastTraderSetup-simu-plugins
Compression=lzma
SolidCompression=yes
VersionInfoCompany=CIFCO ITSER VICES CO.,LTD
VersionInfoTextVersion=1.7.2.30
VersionInfoVersion=1.7.2.30
[Languages]
;Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chs"; MessagesFile: "compiler:ChineseSimp.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\source\commonfiles\FastTrader.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\source\commonfiles\*"; DestDir: "{app}"; Excludes: ".svn,plugin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\source\commonfiles\plugin\ArbitrageDelagation\*"; DestDir: "{app}\plugin\ArbitrageDelagation"; Excludes: ".svn"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\source\commonfiles\plugin\Module-BookOrder\*"; DestDir: "{app}\plugin\Module-BookOrder"; Excludes: ".svn"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\source\commonfiles\plugin\ZqQuotation\*"; DestDir: "{app}\plugin\ZqQuotation"; Excludes: ".svn"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\source\FastTrader-simu-plugins\*"; DestDir: "{app}"; Excludes: ".svn"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\中国国际期货网上交易终端(模拟版-plugins)"; Filename: "{app}\FastTrader.exe"; WorkingDir:"{app}"
Name: "{group}\{cm:UninstallProgram,中国国际期货网上交易终端(模拟版-plugins)}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\中国国际期货网上交易终端(模拟版-plugins)"; Filename: "{app}\FastTrader.exe"; Tasks: desktopicon; WorkingDir:"{app}"

[Run]
Filename: "{app}\FastTrader.exe"; Description: "{cm:LaunchProgram,中国国际期货网上交易终端(模拟版-plugins)}"; Flags: nowait postinstall skipifsilent runascurrentuser

[Code]

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);

begin

if CurUninstallStep = usUninstall then
    DelTree(ExpandConstant('{app}'), True, True, True);

end;






































