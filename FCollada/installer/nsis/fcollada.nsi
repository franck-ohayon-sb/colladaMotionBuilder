; FCollada install script for NSIS
;
; TODO:
; - Link to documentation.
; - Implement uninstall
;
;--------------------------------

; The name of the installer
Name "FCollada"

; Include LogicLib for conditionals (e.g. if)
!include "LogicLib.nsh"
!include "MUI.nsh"
!include "Sections.nsh"

SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES\Feeling Software\FCollada"

; The file to write
OutFile "FCollada Setup.exe"

; The text to prompt the user to enter a directory
DirText "Please select the path where FCollada should be installed."

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\FeelingSoftware\FCollada" "Install_Dir"

; automatically close the installer when done.
;AutoCloseWindow true

; hide the "show details" box
;ShowInstDetails nevershow


;--------------------------------
; Global variables
;--------------------------------

;--------------------------------
; Pages

;Interface Settings
!define MUI_ABORTWARNING

!define MUI_HEADERIMAGE
;!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\nsis.bmp"

!define MUI_COMPONENTSPAGE_SMALLDESC

;Pages
!define MUI_WELCOMEPAGE_TITLE "Welcome to Feeling Software's FCollada Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Feeling Software's FCollada, the COLLADA import/export library.\r\n\r\n$_CLICK"

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
; SECTIONS
;--------------------------------
Section "Source code and documentation"
  SetShellVarContext all ; Install the shortcuts in all users's start-menus.

  SectionIn RO ; required section

  ; Copy everything important.
  SetOutPath $INSTDIR
  File /r /x *.ncb /x *.obj /x *.ilk \
    /x *.pch /x *.user /x *.dep /x *.manifest /x *.bat /x *.bak \
	/x doxygen.exe /x doxytag.exe /x GenerateDoxygen.py \
	/x Package_*.* /x Doxyfile /x Output \
    /x *.pdb /x *.exp /x *.idb /x *.suo /x *.user \
    /x BuildLog.* /x .svn /x NAnt.build /x nunit.* \
    /x *.lib /x *.dll /x *.pcc /x *.enc /x installer \
    ..\..\*
    
  ; This is an important Python script..
  SetOutPath $INSTDIR\..
  File ..\..\..\SconsCommon.py

  ; This covers the FArchiveXML plug-in.
  CreateDirectory "$INSTDIR\..\FColladaPlugins"
  CreateDirectory "$INSTDIR\..\FColladaPlugins\FArchiveXML"
  SetOutPath $INSTDIR\..\FColladaPlugins\FArchiveXML
  File  ..\..\..\FColladaPlugins\FArchiveXML\*.cpp
  File  ..\..\..\FColladaPlugins\FArchiveXML\*.h
  File  ..\..\..\FColladaPlugins\FArchiveXML\*.txt
  File  ..\..\..\FColladaPlugins\FArchiveXML\*.vcproj
  
  ; This covers the Cg toolkit.
  CreateDirectory "$INSTDIR\..\External"
  CreateDirectory "$INSTDIR\..\External\Cg"
  SetOutPath $INSTDIR\..\External\Cg
  File /r /x .svn ..\..\..\External\Cg\*

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\FeelingSoftware\FCollada "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\FCollada" "DisplayName" "Feeling Software FCollada"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\FCollada" "UninstallString" '"$INSTDIR\uninstallFCollada.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\FCollada" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\FCollada" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstallFCollada.exe"
  
  CreateDirectory "$SMPROGRAMS\Feeling Software\FCollada"
  CreateShortCut "$SMPROGRAMS\Feeling Software\FCollada\FCollada Documentation.lnk" "$INSTDIR\Output\Doxygen\html\index.html" "" "$INSTDIR\Output\Doxygen\html\index.html" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\FCollada\Feeling Software.com.lnk" "http://www.feelingsoftware.com" "" "http://www.feelingsoftware.com" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\FCollada\Uninstall.lnk" "$INSTDIR\uninstallFCollada.exe" "" "$INSTDIR\uninstallFCollada.exe" 0  
  
SectionEnd

Section "Example conditioners (FColladaTools)"

  ; Copy samples.
  CreateDirectory "$INSTDIR\..\FColladaTools"
  SetOutPath $INSTDIR\..\FColladaTools
  File /r /x *.ncb /x *.obj /x *.ilk \
    /x *.pch /x *.user /x *.dep /x *.manifest /x *.bat \
	/x doxygen.exe /x doxytag.exe /x GenerateDoxygen.py \
	/x Package_*.* /x Doxyfile \
    /x *.pdb /x *.exp /x *.idb /x *.suo /x *.user \
    /x BuildLog.* /x .svn /x NAnt.build /x nunit.* \
    /x FCollada*.lib /x FMath.lib /x FUtils.lib /x LibXML.lib \
	/x FCollada*.dll /x *.pcc /x *.res /x *.enc \
    /x *.exe \
    ..\..\..\FColladaTools\*

  ; Copy DevIL libraries and DLLs.
  SetOutPath $INSTDIR\..\External\DevIL
  File /r /x .svn ..\..\..\External\DevIL\*

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  SetShellVarContext all ; Un-install the shortcuts from all user's the start menu folder.

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\FCollada"
  DeleteRegKey HKLM SOFTWARE\FeelingSoftware\FCollada

  ; Remove files and uninstaller
  Delete $INSTDIR\uninstallFCollada.exe

  ; Remove Start menu shortcuts.
  RMDir /r "$SMPROGRAMS\Feeling Software\FCollada"
  
  ; Remove the root file(s)
  Delete $INSTDIR\..\SconsCommon.py

  ; Remove directories used
  RMDir /r "$INSTDIR\..\FColladaTools"  ; samples
  RMDir /r "$INSTDIR\..\FColladaPlugins\FArchiveXML"  ; XML archiver
  RMDir /r "$INSTDIR\..\FColladaPlugins" ; Are there other plug-ins, maybe the user put some there?
  RMDir /r "$INSTDIR\..\External"  ; Cg and pre-compiled DevIL for samples
  RMDir /r "$INSTDIR"
  RMDir "$SMPROGRAMS\Feeling Software" ; in case there is no other Feeling Software product installed.

SectionEnd
