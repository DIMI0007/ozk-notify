; Build with NSIS 3 (makensis):
; makensis /DPLUGIN_DLL="C:\path\to\stream-notify.dll" installer\StreamNotify.nsi
Unicode true
!include "MUI2.nsh"
!ifndef PLUGIN_DLL
!error "Pass /DPLUGIN_DLL=absolute_path_to_built_dll"
!endif
!ifndef QT_WEBSOCKETS_DLL
!error "Pass /DQT_WEBSOCKETS_DLL=path_to_matching_Qt6WebSockets.dll"
!endif
Name "OZK Notify 1.0"
OutFile "StreamNotify-1.0.0-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\obs-studio"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "English"

Section "OZK Notify" SecPlugin
  SetRegView 64
  IfFileExists "$INSTDIR\bin\64bit\obs64.exe" found no_obs
  no_obs:
    MessageBox MB_ICONSTOP "OBS Studio x64 is not present in this directory. Select your OBS installation folder."
    Abort
  found:
  ; OBS must be closed by the user before installing.
  CreateDirectory "$INSTDIR\obs-plugins\64bit"
  SetOutPath "$INSTDIR\obs-plugins\64bit"
  File /oname=stream-notify.dll "${PLUGIN_DLL}"
  ; Qt6WebSockets is a runtime dependency and should be found next to obs64.exe.
  ; Do not overwrite a copy OBS/another plugin has already provided.
  IfFileExists "$INSTDIR\bin\64bit\Qt6WebSockets.dll" websocket_present
    SetOutPath "$INSTDIR\bin\64bit"
    File /oname=Qt6WebSockets.dll "${QT_WEBSOCKETS_DLL}"
  websocket_present:

  WriteUninstaller "$INSTDIR\StreamNotify-Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StreamNotify" "DisplayName" "OZK Notify 1.0 for OBS"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StreamNotify" "UninstallString" '"$INSTDIR\StreamNotify-Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StreamNotify" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StreamNotify" "NoModify" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  Delete "$INSTDIR\obs-plugins\64bit\stream-notify.dll"
  ; Leave shared Qt6WebSockets.dll in bin/64bit on uninstall, as other plugins may use it.
  Delete "$INSTDIR\StreamNotify-Uninstall.exe"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StreamNotify"
  ; User settings remain in OBS profile by design (do not destroy tokens on uninstall).
SectionEnd
