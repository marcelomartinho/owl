; Owl Browser - NSIS Installer Script
; Generates OwlSetup.exe for Windows installation.
;
; Prerequisites:
;   - NSIS 3.x installed (https://nsis.sourceforge.io/)
;   - Owl browser built (chrome.exe in build output dir)
;
; Usage:
;   makensis /DBUILD_DIR=path\to\out\Owl installer\owl_installer.nsi
;
; Output:
;   OwlSetup.exe in the installer\ directory

!ifndef BUILD_DIR
  !define BUILD_DIR "..\chromium\src\out\Owl"
!endif

; ---- General ----
!define PRODUCT_NAME "Owl Browser"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "Owl Browser Project"
!define PRODUCT_WEB_SITE "https://github.com/marcelomartinho/owl"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY "Software\OwlBrowser"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "OwlSetup.exe"
InstallDir "$PROGRAMFILES64\Owl Browser"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin

; ---- Compression ----
SetCompressor /SOLID lzma
SetCompressorDictSize 64

; ---- Modern UI ----
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\branding\owl.ico"
!define MUI_UNICON "..\branding\owl.ico"

; ---- Pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\owl.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Owl Browser"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ---- Language ----
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "PortugueseBR"

; ---- Install Section ----
Section "Owl Browser (required)" SEC_MAIN
  SectionIn RO

  SetOutPath "$INSTDIR"

  ; Copy the main executable (renamed from chrome.exe to owl.exe)
  CopyFiles /SILENT "${BUILD_DIR}\chrome.exe" "$INSTDIR\owl.exe"

  ; Copy all required DLLs and resources from the build directory
  File /r /x "*.pdb" /x "*.lib" /x "*.exp" /x "gen" /x "obj" "${BUILD_DIR}\*.dll"
  File /r "${BUILD_DIR}\*.pak"
  File /r "${BUILD_DIR}\*.dat"
  File /r "${BUILD_DIR}\*.bin"

  ; Copy ICU data
  File /nonfatal "${BUILD_DIR}\icudtl.dat"

  ; Copy V8 snapshot
  File /nonfatal "${BUILD_DIR}\v8_context_snapshot.bin"
  File /nonfatal "${BUILD_DIR}\snapshot_blob.bin"

  ; Copy locales
  SetOutPath "$INSTDIR\locales"
  File /r "${BUILD_DIR}\locales\*.pak"

  ; Copy resources directory
  SetOutPath "$INSTDIR"
  File /nonfatal /r "${BUILD_DIR}\resources"

  ; Copy Widevine (for DRM content like Netflix)
  File /nonfatal /r "${BUILD_DIR}\WidevineCdm"

  ; Copy branding assets
  File /nonfatal "..\branding\owl.ico"

  ; Write version info file
  FileOpen $0 "$INSTDIR\version.txt" w
  FileWrite $0 "${PRODUCT_VERSION}"
  FileClose $0

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Registry keys for Add/Remove Programs
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\owl.exe"'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR"

  ; Calculate installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

; ---- Shortcuts Section ----
Section "Desktop Shortcut" SEC_DESKTOP
  CreateShortCut "$DESKTOP\Owl Browser.lnk" "$INSTDIR\owl.exe" "" "$INSTDIR\owl.ico"
SectionEnd

Section "Start Menu Shortcuts" SEC_STARTMENU
  CreateDirectory "$SMPROGRAMS\Owl Browser"
  CreateShortCut "$SMPROGRAMS\Owl Browser\Owl Browser.lnk" "$INSTDIR\owl.exe" "" "$INSTDIR\owl.ico"
  CreateShortCut "$SMPROGRAMS\Owl Browser\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

; ---- File Associations Section ----
Section "Register as Web Browser" SEC_REGISTER
  ; Register URL protocols
  WriteRegStr HKLM "Software\Classes\OwlBrowserHTML\DefaultIcon" "" "$INSTDIR\owl.exe,0"
  WriteRegStr HKLM "Software\Classes\OwlBrowserHTML\shell\open\command" "" '"$INSTDIR\owl.exe" "%1"'

  ; HTTP protocol
  WriteRegStr HKLM "Software\Classes\owl\shell\open\command" "" '"$INSTDIR\owl.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\owl" "URL Protocol" ""

  ; Register in Windows Default Programs
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser" "" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\DefaultIcon" "" "$INSTDIR\owl.exe,0"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\shell\open\command" "" "$INSTDIR\owl.exe"

  ; Register capabilities
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities" "ApplicationName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities" "ApplicationDescription" "Memory-optimized Chromium browser"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities\URLAssociations" "http" "OwlBrowserHTML"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities\URLAssociations" "https" "OwlBrowserHTML"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities\FileAssociations" ".htm" "OwlBrowserHTML"
  WriteRegStr HKLM "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities\FileAssociations" ".html" "OwlBrowserHTML"

  WriteRegStr HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}" "Software\Clients\StartMenuInternet\OwlBrowser\Capabilities"
SectionEnd

; ---- Uninstaller ----
Section "Uninstall"
  ; Kill running instances
  nsExec::ExecToLog 'taskkill /F /IM owl.exe'

  ; Remove files
  RMDir /r "$INSTDIR"

  ; Remove shortcuts
  Delete "$DESKTOP\Owl Browser.lnk"
  RMDir /r "$SMPROGRAMS\Owl Browser"

  ; Remove registry keys
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKLM "Software\Classes\OwlBrowserHTML"
  DeleteRegKey HKLM "Software\Classes\owl"
  DeleteRegKey HKLM "Software\Clients\StartMenuInternet\OwlBrowser"
  DeleteRegValue HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}"
SectionEnd

; ---- Section Descriptions ----
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} "Core Owl Browser files (required)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Create a shortcut on your Desktop."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU} "Create Start Menu shortcuts."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_REGISTER} "Register Owl as an available web browser in Windows Settings."
!insertmacro MUI_FUNCTION_DESCRIPTION_END
