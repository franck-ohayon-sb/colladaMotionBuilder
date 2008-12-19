; ColladaMax install script for NSIS
;
; TODO:
; - Warn the user if installing source code but FCollada is not installed (or old version)
; - What happens if a version of 3ds Max is not installed? Need to disable install section.
; - Fix uninstall, which doesn't delete all the installed files and folders.
;
;--------------------------------

; The name of the installer
Name "ColladaMax"

; Include LogicLib for conditionals (e.g. if)
!include "LogicLib.nsh"
!include "MUI.nsh"
!include "Sections.nsh"

SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES\Feeling Software\ColladaMax"

; The file to write
OutFile "ColladaMax Setup.exe"

; The text to prompt the user to enter a directory
DirText "Please select the path where ColladaMax's documentation and source code should be installed."

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\FeelingSoftware\ColladaMax" "Install_Dir"

; automatically close the installer when done.
;AutoCloseWindow true

; hide the "show details" box
;ShowInstDetails nevershow


;--------------------------------
; Global variables
;--------------------------------

Var MAX_70_DIR
Var MAX_80_DIR
Var MAX_90_DIR
Var MAX_2008_DIR
Var MAX_2009_DIR

Var WINDOWS_SYSTEM

Var WANTS_CG_UPGRADE

;--------------------------------
; Pages

;Interface Settings
!define MUI_ABORTWARNING

!define MUI_HEADERIMAGE
;!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\nsis.bmp"

!define MUI_COMPONENTSPAGE_SMALLDESC

;Pages
!define MUI_WELCOMEPAGE_TITLE "Welcome to Feeling Software's ColladaMax Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Feeling Software's ColladaMax, the COLLADA plug-in for Autodesk 3ds Max.\r\n\r\n$_CLICK"

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

	; Check for the current system (32 or 64 bit)
    System::Call "kernel32::GetCurrentProcess() i .s"
  	System::Call "kernel32::IsWow64Process(i s, *i .r0)"
  	StrCpy $WINDOWS_SYSTEM "32bit"
  	IntCmp $0 0 systemOutput
  	StrCpy $WINDOWS_SYSTEM "64bit"
	StrCpy $INSTDIR "$PROGRAMFILES64\Feeling Software\ColladaMaya"
	SetRegView 64
systemOutput:
	MessageBox MB_OK "Windows system found: $WINDOWS_SYSTEM"

    ; Check for version 7.0
    ReadRegStr $MAX_70_DIR HKLM "Software\Autodesk\3dsMax\7.0" "InstallDir"
    StrCmp $MAX_70_DIR "" NotFound70
        MessageBox MB_OK "3ds Max 7.0 was found at: $MAX_70_DIR"
    NotFound70:
    
    ; Check for version 8.0
    ReadRegStr $MAX_80_DIR HKLM "Software\Autodesk\3dsMax\8.0" "InstallDir"
    StrCmp $MAX_80_DIR "" NotFound80
        MessageBox MB_OK "3ds Max 8.0 was found at: $MAX_80_DIR"
    NotFound80:
    
    ; Check for version 9.0
    ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:409" "InstallDir"
    StrCmp $MAX_90_DIR "" NotFound90_0 Found90
	NotFound90_0:
	    ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:410" "InstallDir"
		StrCmp $MAX_90_DIR "" NotFound90_1 Found90
		NotFound90_1:
			ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:411" "InstallDir"
			StrCmp $MAX_90_DIR "" NotFound90_2 Found90
            NotFound90_2:
                ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:408" "InstallDir"
                StrCmp $MAX_90_DIR "" NotFound90_3 Found90
                NotFound90_3:
                    ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:407" "InstallDir"
                    StrCmp $MAX_90_DIR "" NotFound90_4 Found90
	Found90:
        MessageBox MB_OK "3ds Max 9.0 was found at: $MAX_90_DIR"
	NotFound90_4:
    
    ; Check for version (10.0) 2008
    ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:409" "InstallDir"
    StrCmp $MAX_2008_DIR "" NotFound2008_0 Found2008
	NotFound2008_0:
	    ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:410" "InstallDir"
		StrCmp $MAX_2008_DIR "" NotFound2008_1 Found2008
		NotFound2008_1:
			ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:411" "InstallDir"
			StrCmp $MAX_2008_DIR "" NotFound2008_2 Found2008
    		NotFound2008_2:
                ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:408" "InstallDir"
                StrCmp $MAX_2008_DIR "" NotFound2008_3 Found2008
                NotFound2008_3:
                    ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:407" "InstallDir"
                    StrCmp $MAX_2008_DIR "" NotFound2008_4 Found2008
	Found2008:
        MessageBox MB_OK "3ds Max 2008 was found at: $MAX_2008_DIR"
	NotFound2008_4:

    ; Check for version (11.0) 2009
    ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:409" "InstallDir"
    StrCmp $MAX_2009_DIR "" NotFound2009_0 Found2009
	NotFound2009_0:
	    ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:410" "InstallDir"
		StrCmp $MAX_2009_DIR "" NotFound2009_1 Found2009
		NotFound2009_1:
			ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:411" "InstallDir"
			StrCmp $MAX_2009_DIR "" NotFound2009_2 Found2009
    		NotFound2009_2:
                ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:408" "InstallDir"
                StrCmp $MAX_2009_DIR "" NotFound2009_3 Found2009
                NotFound2009_3:
                    ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:407" "InstallDir"
                    StrCmp $MAX_2009_DIR "" NotFound2009_4 Found2009
	Found2009:
        MessageBox MB_OK "3ds Max 2009 was found at: $MAX_2009_DIR"
	NotFound2009_4:

    
	; TODO: If no version of 3ds Max has been detected, warn the user.
    ${If} $MAX_70_DIR == ""
    ${AndIf} $MAX_80_DIR == ""
    ${AndIf} $MAX_90_DIR == ""
    ${AndIf} $MAX_2008_DIR == ""
    ${AndIf} $MAX_2009_DIR == ""
      MessageBox MB_OK 'Warning: This plug-in is compiled for Autodesk 3ds Max 2009, 2008, 9.0, 8.0 and 7.0.\nNeither were detected. You can still install the source code\nand documentation, and copy the plug-in files manually.'
    ${EndIf}

FunctionEnd 

;--------------------------------
; SECTIONS
;--------------------------------
Section "Documentation"

  SectionIn RO
  
  SetShellVarContext all

  ; Copy plug-in source code and docs
  SetOutPath $INSTDIR
  CreateDirectory "$INSTDIR"
	
  ${If} $WINDOWS_SYSTEM == "64bit"
		SetRegView 64
  ${Endif}
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\FeelingSoftware\ColladaMax "Install_Dir" "$INSTDIR"
  CreateDirectory "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMax" "DisplayName" "Feeling Software ColladaMax"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMax" "UninstallString" '"$INSTDIR\uninstallColladaMax.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMax" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMax" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstallColladaMax.exe"
  
  SetRegView 32

SectionEnd

Section "Install Localized CG (recommended)"
  StrCpy $WANTS_CG_UPGRADE "Yes"
SectionEnd

Section "Source"

  ; Copy plug-in source code. Ignore a bunch of useless files.
  SetOutPath $INSTDIR
  File /r /x "ColladaMax*Setup.exe" /x *.ncb /x .mayaSwatches \
    /x .svn /x *.suo /x *.bat /x *.user \
    /x Output /x installer /x *.bak ..\..\*
  
SectionEnd

;--------------------------------

; The stuff to install
Section "Plug-in for 3ds Max 7.0"

  ; Skip this section if 3ds Max 7.0 is not installed
  StrCmp $MAX_70_DIR "" End70

  ; Copy necessary D3DX DLLs.
  SetOutPath $MAX_70_DIR
  File ..\..\..\External\D3D\D3DX9_30.dll
  File ..\..\..\External\D3D\D3DX9_34.dll

  ; Copy plug-in itself
  SetOutPath $MAX_70_DIR\plugins
  File "..\..\Output\Release 7 Win32\ColladaMax\ColladaMax.dle"

  ; Take out Autodesk' version of the plug-in,
  Delete $MAX_70_DIR\plugins\ColladaExporter.dle
  Delete $MAX_70_DIR\plugins\ColladaImporter.dli
  
  ; Take out the old ColladaMax script plug-in.
  Delete $MAX_70_DIR\plugins\ColladaMaxScrpt.dlx
 
  ${If} $WANTS_CG_UPGRADE == "Yes"
    ; Copy Cg DLLs.
    SetOutPath $MAX_70_DIR
    File ..\..\..\External\Cg\bin\cg.dll
    File ..\..\..\External\Cg\bin\cgD3D9.dll
   ${Endif}

  End70:

SectionEnd

; The stuff to install
Section "Plug-in for 3ds Max 8.0"

  ; Skip this section if 3ds Max 8.0 is not installed
  StrCmp $MAX_80_DIR "" End80

  ; Copy necessary D3DX DLLs.
  SetOutPath $MAX_80_DIR
  File ..\..\..\External\D3D\D3DX9_30.dll
  File ..\..\..\External\D3D\D3DX9_34.dll

  ; Copy plug-in itself
  SetOutPath $MAX_80_DIR\plugins
  File "..\..\Output\Release 8 Win32\ColladaMax\ColladaMax.dle"
 
  ; Take out Autodesk' version of the plug-in,
  Delete $MAX_80_DIR\plugins\ColladaExporter.dle
  Delete $MAX_80_DIR\plugins\ColladaImporter.dli
  
  ; Take out the old ColladaMax script plug-in.
  Delete $MAX_80_DIR\plugins\ColladaMaxScrpt.dlx

  ${If} $WANTS_CG_UPGRADE == "Yes"
    ; Copy Cg DLLs.
    SetOutPath $MAX_80_DIR
    File ..\..\..\External\Cg\bin\cg.dll
    File ..\..\..\External\Cg\bin\cgD3D9.dll
  ${Endif}

End80:
  
SectionEnd

; The stuff to install
Section "Plug-in for 3ds Max 9.0"

  ; Skip this section if 3ds Max 9.0 is not installed
  StrCmp $MAX_90_DIR "" End90

  ; Copy necessary D3DX DLLs.
  SetOutPath $MAX_90_DIR
  File ..\..\..\External\D3D\D3DX9_30.dll
  File ..\..\..\External\D3D\D3DX9_34.dll

  ; Copy plug-in itself
  SetOutPath $MAX_90_DIR\plugins
  File "..\..\Output\Release 9 Win32\ColladaMax\ColladaMax.dle"

  ; Take out Autodesk' version of the plug-in,
  Delete $MAX_90_DIR\plugins\ColladaExporter.dle
  Delete $MAX_90_DIR\plugins\ColladaImporter.dli
  
  ; Take out the old ColladaMax script plug-in.
  Delete $MAX_90_DIR\plugins\ColladaMaxScrpt.dlx

  ${If} $WANTS_CG_UPGRADE == "Yes"
    ; Copy Cg DLLs.
    SetOutPath $MAX_90_DIR
    File ..\..\..\External\Cg\bin\cg.dll
    File ..\..\..\External\Cg\bin\cgD3D9.dll
  ${Endif}

  End90:
  
SectionEnd

; The stuff to install
Section "Plug-in for 3ds Max 2008"

  ; Skip this section if 3ds Max 2008 is not installed
  StrCmp $MAX_2008_DIR "" End2008

  ; Copy necessary D3DX DLLs.
  ${If} $WINDOWS_SYSTEM == "32bit"
  SetOutPath $MAX_2008_DIR
  File ..\..\..\External\D3D\D3DX9_30.dll
  File ..\..\..\External\D3D\D3DX9_34.dll
  ${Endif}

  ; Copy plug-in itself
  SetOutPath $MAX_2008_DIR\plugins

  ${If} $WINDOWS_SYSTEM == "32bit"
    File "..\..\Output\Release 2008 Win32\ColladaMax\ColladaMax.dle"
  ${Endif}
  ${If} $WINDOWS_SYSTEM == "64bit"
    File "..\..\Output\Release 2008 x64\ColladaMax\ColladaMax.dle"
  ${Endif}

  ; Take out Autodesk' version of the plug-in,
  Delete $MAX_2008_DIR\plugins\ColladaExporter.dle
  Delete $MAX_2008_DIR\plugins\ColladaImporter.dli
  
  ; Take out the old ColladaMax script plug-in.
  Delete $MAX_2008_DIR\plugins\ColladaMaxScrpt.dlx


  ${If} $WANTS_CG_UPGRADE == "Yes"
    ; Copy Cg DLLs.
    ${If} $WINDOWS_SYSTEM == "32bit"
    SetOutPath $MAX_2008_DIR
    File ..\..\..\External\Cg\bin\cg.dll
    File ..\..\..\External\Cg\bin\cgD3D9.dll
    ${Endif}
  ${Endif}

  End2008:

SectionEnd


; The stuff to install
Section "Plug-in for 3ds Max 2009"

  ; Skip this section if 3ds Max 2009 is not installed
  StrCmp $MAX_2009_DIR "" End2009

  ; Copy necessary D3DX DLLs.
  ${If} $WINDOWS_SYSTEM == "32bit"
  SetOutPath $MAX_2009_DIR
  File ..\..\..\External\D3D\D3DX9_30.dll
  File ..\..\..\External\D3D\D3DX9_34.dll
  ${Endif}

  ; Copy plug-in itself
  SetOutPath $MAX_2009_DIR\plugins
  ${If} $WINDOWS_SYSTEM == "32bit"
  File "..\..\Output\Release 2009 Win32\ColladaMax\ColladaMax.dle"
  ${Endif}
  ${If} $WINDOWS_SYSTEM == "64bit"
    File "..\..\Output\Release 2009 x64\ColladaMax\ColladaMax.dle"
  ${Endif}


  ; Take out Autodesk' version of the plug-in,
  Delete $MAX_2009_DIR\plugins\ColladaExporter.dle
  Delete $MAX_2009_DIR\plugins\ColladaImporter.dli
  
  ; Take out the old ColladaMax script plug-in.
  Delete $MAX_2009_DIR\plugins\ColladaMaxScrpt.dlx

  ${If} $WANTS_CG_UPGRADE == "Yes"
    ; Copy Cg DLLs.
    ${If} $WINDOWS_SYSTEM == "32bit"
    SetOutPath $MAX_2009_DIR
    File ..\..\..\External\Cg\bin\cg.dll
    File ..\..\..\External\Cg\bin\cgD3D9.dll
    ${Endif}
  ${Endif}

  End2009:

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Feeling Software\ColladaMax"
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMax\Online ColladaMax Documentation.lnk" "http://www.feelingsoftware.com/content/view/65/79/lang,en/" "" "http://www.feelingsoftware.com/content/view/65/79/lang,en/" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMax\Release Notes (English).lnk" "$INSTDIR\Release notes.doc" "" "$INSTDIR\Release notes.doc" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMax\Release Notes (Japanese).lnk" "$INSTDIR\Release notes JP.doc" "" "$INSTDIR\Release notes JP.doc" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMax\Feeling Software.com.lnk" "http://www.feelingsoftware.com" "" "http://www.feelingsoftware.com" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMax\Uninstall.lnk" "$INSTDIR\uninstallColladaMax.exe" "" "$INSTDIR\uninstallColladaMax.exe" 0  
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  SetShellVarContext all ; Uninstall the shortcuts in all users's start-menus.

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMax"
  DeleteRegKey HKLM SOFTWARE\FeelingSoftware\ColladaMax

  ; Remove files and uninstaller
  Delete "$INSTDIR\uninstallColladaMax.exe"
  
  ; Remove plug-ins, if any.
  ; Skip this section if 3ds Max 7.0 is not installed
  ReadRegStr $MAX_70_DIR HKLM "Software\Autodesk\3dsMax\7.0" "InstallDir"
  StrCmp $MAX_70_DIR "" UninstallMax70Finished
  Delete "$MAX_70_DIR\plugins\ColladaMax.dle"
  UninstallMax70Finished:

  ; Skip this section if 3ds Max 8.0 is not installed
  ReadRegStr $MAX_80_DIR HKLM "Software\Autodesk\3dsMax\8.0" "InstallDir"
  StrCmp $MAX_80_DIR "" UninstallMax80Finished
  Delete "$MAX_80_DIR\plugins\ColladaMax.dle"
  UninstallMax80Finished:

  ; Skip this section if 3ds Max 9.0 is not installed
  ReadRegStr $MAX_90_DIR HKLM "Software\Autodesk\3dsMax\9.0\MAX-1:409" "InstallDir"
  StrCmp $MAX_90_DIR "" UninstallMax90Finished
  Delete "$MAX_90_DIR\plugins\ColladaMax.dle"
  UninstallMax90Finished:
  
  ReadRegStr $MAX_2008_DIR HKLM "Software\Autodesk\3dsMax\10.0\MAX-1:409" "InstallDir"
  StrCmp $MAX_2008_DIR "" UninstallMax2008Finished
  Delete "$MAX_2008_DIR\plugins\ColladaMax.dle"
  UninstallMax2008Finished:

  ; Skip this section if 3ds Max 2009 is not installed
  ReadRegStr $MAX_2009_DIR HKLM "Software\Autodesk\3dsMax\11.0\MAX-1:409" "InstallDir"
  StrCmp $MAX_2009_DIR "" UninstallMax2009Finished
  Delete "$MAX_2009_DIR\plugins\ColladaMax.dle"
  UninstallMax2009Finished:

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Feeling Software\ColladaMax\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Feeling Software\ColladaMax"
  RMDir "$SMPROGRAMS\Feeling Software" ; in case there is no other Feeling Software product installed.
  RMDir /r "$INSTDIR"

SectionEnd
