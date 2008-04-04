; ColladaMotionBuilder install script for NSIS
;
;--------------------------------

; The name of the installer
Name "ColladaMotionBuilder"

; Include LogicLib for conditionals (e.g. if)
!include "LogicLib.nsh"
!include "MUI.nsh"
!include "Sections.nsh"

SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES\Feeling Software\ColladaMotionBuilder"

; The file to write
OutFile "ColladaMotionBuilder Setup.exe"

; The text to prompt the user to enter a directory
DirText "Please select the path where ColladaMotionBuilder's documentation and source code should be installed."

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\FeelingSoftware\ColladaMotionBuilder" "Install_Dir"

; automatically close the installer when done.
;AutoCloseWindow true

; hide the "show details" box
;ShowInstDetails nevershow

;--------------------------------
; Global variables
;--------------------------------
Var MB_75_DIR
Var MB_75_PLUGIN_DIR

Var MB_70_DIR
Var MB_70_PLUGIN_DIR

;--------------------------------
; Pages

;Interface Settings
!define MUI_ABORTWARNING

!define MUI_HEADERIMAGE
;!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\nsis.bmp"

!define MUI_COMPONENTSPAGE_SMALLDESC

;Pages
!define MUI_WELCOMEPAGE_TITLE "Welcome to Feeling Software's ColladaMotionBuilder Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Feeling Software's ColladaMotionBuilder, the COLLADA plug-in for Autodesk MotionBuilder.\r\n\r\n$_CLICK"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\Pre-requisites and License.rtf"
;!ifdef VER_MAJOR & VER_MINOR & VER_REVISION & VER_BUILD
;Page custom PageReinstall PageLeaveReinstall
;!endif
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_LINK "Visit the Feeling Software site for the latest news!"
!define MUI_FINISHPAGE_LINK_LOCATION "http://www.feelingsoftware.com"

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES


;--------------------------------
;Languages

!insertmacro MUI_LANGUAGE "English"


;--------------------------------
; CALLBACKS
;--------------------------------
Function .onInit

    ; Check for version 7.5
    ReadRegStr $MB_75_DIR HKLM "SOFTWARE\Alias\MotionBuilder Family\7.50.0000" "LastInstallPath"
    StrCmp $MB_75_DIR "" NotFound75
        MessageBox MB_OK "MotionBuilder 7.5 was found at: $MB_75_DIR"
    NotFound75:

    ; Check for version 7.0
    ReadRegStr $MB_70_DIR HKLM "SOFTWARE\Alias\MotionBuilder Family\7.00.0000" "LastInstallPath"
    StrCmp $MB_70_DIR "" NotFound70
        MessageBox MB_OK "MotionBuilder 7.0 was found at: $MB_70_DIR"
    NotFound70:
    
    ; TODO: If no version of MotionBuilder has been detected, warn the user.
    ${If} $MB_70_DIR == ""
    ${AndIf} $MB_75_DIR == ""
      MessageBox MB_OK 'Warning: This plug-in is compiled for MotionBuilder 7.0 and 7.5, but no such version was detected. You can still install the source code and documentation, and copy the plug-in files manually.'
    ${EndIf}

FunctionEnd 

;--------------------------------
; SECTIONS
;--------------------------------
Section "Documentation"
  SetShellVarContext all ; Install the shortcuts in all users's start-menus.

  SectionIn RO

  ; Copy plug-in source code and docs
  SetOutPath $INSTDIR
  CreateDirectory "$INSTDIR"
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\FeelingSoftware\ColladaMotionBuilder "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMotionBuilder" "DisplayName" "Feeling Software ColladaMotionBuilder"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMotionBuilder" "UninstallString" '"$INSTDIR\uninstallColladaMotionBuilder.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMotionBuilder" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMotionBuilder" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstallColladaMotionBuilder.exe"
  
SectionEnd

Section "Source"

  ; Copy plug-in source code. Ignore a bunch of useless files.
  SetOutPath $INSTDIR
  File ..\..\*.cpp
  File ..\..\*.h
  File ..\..\*.rtf
  File ..\..\*.txt
  File ..\..\*.sln
  File ..\..\*.vcproj
  
SectionEnd

;--------------------------------

; The stuff to install
Section "Plug-in for MotionBuilder 7.5"

    ; Skip this section if MotionBuilder 7.5 is not installed
    StrCmp $MB_75_DIR "" End75
    
    StrCpy $MB_75_PLUGIN_DIR "$MB_75_DIRbin\plugins"
    
    ; Copy plug-in itself
    SetOutPath $MB_75_PLUGIN_DIR
    File "..\..\Output\MB7.5 Release\ColladaMotionBuilder.dll"
    
    End75:
    
SectionEnd

Section "Plug-in for MotionBuilder 7.0"

    ; Skip this section if MotionBuilder 7.0 is not installed
    StrCmp $MB_70_DIR "" End70
    
    StrCpy $MB_70_PLUGIN_DIR "$MB_70_DIR\bin\plugins"
    
    ; Copy plug-in itself
    SetOutPath $MB_70_PLUGIN_DIR
    File "..\..\Output\MB7.0 Release\ColladaMotionBuilder.dll"
    
    End70:

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder"
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder\Feeling Software.com.lnk" "http://www.feelingsoftware.com" "" "http://www.feelingsoftware.com" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder\Readme.txt.lnk" "$INSTDIR\Readme.txt" "$INSTDIR\Readme.txt" 0
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder\Uninstall.lnk" "$INSTDIR\uninstallColladaMotionBuilder.exe" "" "$INSTDIR\uninstallColladaMotionBuilder.exe" 0  
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  SetShellVarContext all ; Uninstall the shortcuts in all users's start-menus.

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMotionBuilder"
  DeleteRegKey HKLM SOFTWARE\FeelingSoftware\ColladaMotionBuilder

  ; Remove files and uninstaller
  Delete $INSTDIR\uninstallColladaMotionBuilder.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Feeling Software\ColladaMotionBuilder"
  RMDir "$SMPROGRAMS\Feeling Software" ; in case there is no other Feeling Software product installed.
  RMDir /r "$INSTDIR"

SectionEnd
