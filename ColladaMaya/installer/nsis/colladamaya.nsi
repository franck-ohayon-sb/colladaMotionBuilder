; ColladaMaya install script for NSIS
;
; TODO:
; - Warn the user if installing source code but FCollada is not installed (or old version)
; - What happens if a version of Maya is not installed? Need to disable install section.
; - Fix uninstall, which doesn't delete all the installed files and folders.
; - Restore old Cg libs.
;
;--------------------------------

; The name of the installer
Name "ColladaMaya"

; Include LogicLib for conditionals (e.g. if)
!include "LogicLib.nsh"
!include "MUI.nsh"
!include "Sections.nsh"

SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES\Feeling Software\ColladaMaya"

; The file to write
OutFile "ColladaMaya Setup.exe"

; The text to prompt the user to enter a directory
DirText "Please select the path where ColladaMaya's documentation and source code should be installed."

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\FeelingSoftware\ColladaMaya" "Install_Dir"

; automatically close the installer when done.
;AutoCloseWindow true

; hide the "show details" box
;ShowInstDetails nevershow


;--------------------------------
; Global variables
;--------------------------------
Var MAYA_2009_DIR
Var MAYA_2008_DIR
Var MAYA_85_DIR
Var MAYA_80_DIR
Var MAYA_70_DIR

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
!define MUI_WELCOMEPAGE_TITLE "Welcome to Feeling Software's ColladaMaya Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Feeling Software's ColladaMaya, the COLLADA plug-in for Autodesk Maya.\r\n\r\n$_CLICK"

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
  	

    ; Check for version 2009
    ReadRegStr $MAYA_2009_DIR HKLM "Software\Autodesk\Maya\2009\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
    StrCmp $MAYA_2009_DIR "" NotFound2009
        MessageBox MB_OK "Maya 2009 was found at: $MAYA_2009_DIR"
    NotFound2009:

    ; Check for version 2008
    ReadRegStr $MAYA_2008_DIR HKLM "Software\Autodesk\Maya\2008\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
    StrCmp $MAYA_2008_DIR "" NotFound2008
        MessageBox MB_OK "Maya 2008 was found at: $MAYA_2008_DIR"
    NotFound2008:

    ; Check for version 8.5
    ReadRegStr $MAYA_85_DIR HKLM "Software\Autodesk\Maya\8.5\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
    StrCmp $MAYA_85_DIR "" NotFound85
        MessageBox MB_OK "Maya 8.5 was found at: $MAYA_85_DIR"
    NotFound85:

	; Check for version 8.0
    ReadRegStr $MAYA_80_DIR HKLM "Software\Alias\Maya\8.0\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
    StrCmp $MAYA_80_DIR "" NotFound80
        MessageBox MB_OK "Maya 8.0 was found at: $MAYA_80_DIR"
    NotFound80:
    
    ; Check for version 7.0
    ReadRegStr $MAYA_70_DIR HKLM "Software\Alias\Maya\7.0\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
    StrCmp $MAYA_70_DIR "" NotFound70
        MessageBox MB_OK "Maya 7.0 was found at: $MAYA_70_DIR"
    NotFound70:
    
    ; TODO: If no version of Maya has been detected, warn the user.
    ${If} $MAYA_70_DIR == ""
    ${AndIf} $MAYA_80_DIR == ""
    ${AndIf} $MAYA_85_DIR == ""
    ${AndIf} $MAYA_2008_DIR == ""
    ${AndIf} $MAYA_2009_DIR == ""
      MessageBox MB_OK 'Warning: This plug-in is compiled for Maya 7.0, 8.0, 8.5, 2008 and 2009, but no such version was detected. You can still install the source code and documentation, and copy the plug-in files manually.'
    ${EndIf}
    
    
   	SetRegView 32
    
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
	
	${If} $WINDOWS_SYSTEM == "64bit"
		SetRegView 64
	${Endif}
	
	; Write the installation path into the registry
	WriteRegStr HKLM SOFTWARE\FeelingSoftware\ColladaMaya "Install_Dir" "$INSTDIR"
	
	; Write the uninstall keys for Windows
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMaya" "DisplayName" "Feeling Software ColladaMaya"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMaya" "UninstallString" '"$INSTDIR\uninstallColladaMaya.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMaya" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMaya" "NoRepair" 1
	WriteUninstaller "$INSTDIR\uninstallColladaMaya.exe"
  
   	SetRegView 32

SectionEnd

Section "Source"

  ; Copy plug-in source code. Ignore a bunch of useless files.
  SetOutPath $INSTDIR
  File /r /x *.ncb /x .mayaSwatches /x "Profiling Session" \
    /x *.swatch /x COLLADAexport.lib /x *.suo /x *.bak \
	/x Output /x installer /x *.user /x .svn /x *.bat \
	..\..\*
  
SectionEnd

Section "Overwrite Localized CG (necessary for ColladaFX)"
	StrCpy $WANTS_CG_UPGRADE "Yes"
SectionEnd

;--------------------------------
; INSTALL STUFF
;--------------------------------

; The stuff to install
Section "Plug-in for Maya 2009"

    ; Skip this section if Maya 8.5 is not installed
    StrCmp $MAYA_2009_DIR "" End2009
    
    ; Copy plug-in itself
    SetOutPath $MAYA_2009_DIR\bin\plug-ins
    ${If} $WINDOWS_SYSTEM == "32bit"
    	File "..\..\Output\Maya 2009 Win32\COLLADA.mll"
    ${Endif}
    ${If} $WINDOWS_SYSTEM == "64bit"
    	File "..\..\Output\Maya 2009 x64\COLLADA.mll"
    ${Endif}
    
    ; Copy regular scripts (all mel files except AE templates)
    SetOutPath $MAYA_2009_DIR\scripts\others
	Delete $MAYA_2009_DIR\scripts\others\AEcolladafxPassesTemplate.mel
	Delete $MAYA_2009_DIR\scripts\others\AEcolladafxShaderTemplate.mel
    File /r /x AE*.mel ..\..\scripts\*.mel
    
    ; Copy AE templates scripts
    SetOutPath $MAYA_2009_DIR\scripts\AETemplates
    File ..\..\scripts\AE*.mel
    
    ; Copy XPM icons
    SetOutPath $MAYA_2009_DIR\icons
    File ..\..\scripts\*.xpm
    
	${If} $WANTS_CG_UPGRADE == "Yes"
		; Copy Cg DLLs.
		; Note that this will overwrite existing files. We back them up
		; first, if the backup don't already exist.
		IfFileExists "$MAYA_2009_DIR\bin\Cg.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2009_DIR\bin\Cg.dll" "$MAYA_2009_DIR\bin\Cg.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_2009_DIR\bin\CgGL.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2009_DIR\bin\CgGL.dll" "$MAYA_2009_DIR\bin\CgGL.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_2009_DIR\bin\glut32.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2009_DIR\bin\glut32.dll" "$MAYA_2009_DIR\bin\glut32.backup_before_ColladaMaya.dll"

		SetOutPath $MAYA_2009_DIR\bin
	    ${If} $WINDOWS_SYSTEM == "32bit"
			File ..\..\..\External\Cg\bin\cg.dll
			File ..\..\..\External\Cg\bin\cgGL.dll
			File ..\..\..\External\Cg\bin\glut32.dll
	    ${Endif}
	    ${If} $WINDOWS_SYSTEM == "64bit"
			File ..\..\..\External\Cg\bin.x64\cg.dll
			File ..\..\..\External\Cg\bin.x64\cgGL.dll
			File ..\..\..\External\Cg\bin.x64\glut32.dll
	    ${Endif}
	${Endif}
    
    End2009:
    
SectionEnd


; The stuff to install
Section "Plug-in for Maya 2008"

    ; Skip this section if Maya 8.5 is not installed
    StrCmp $MAYA_2008_DIR "" End2008
    
    ; Copy plug-in itself
    SetOutPath $MAYA_2008_DIR\bin\plug-ins
    ${If} $WINDOWS_SYSTEM == "32bit"
    	File "..\..\Output\Maya 2008 Win32\COLLADA.mll"
    ${Endif}
    ${If} $WINDOWS_SYSTEM == "64bit"
    	File "..\..\Output\Maya 2008 x64\COLLADA.mll"
    ${Endif}
    
    ; Copy regular scripts (all mel files except AE templates)
    SetOutPath $MAYA_2008_DIR\scripts\others
	Delete $MAYA_2008_DIR\scripts\others\AEcolladafxPassesTemplate.mel
	Delete $MAYA_2008_DIR\scripts\others\AEcolladafxShaderTemplate.mel
    File /r /x AE*.mel ..\..\scripts\*.mel
    
    ; Copy AE templates scripts
    SetOutPath $MAYA_2008_DIR\scripts\AETemplates
    File ..\..\scripts\AE*.mel
    
    ; Copy XPM icons
    SetOutPath $MAYA_2008_DIR\icons
    File ..\..\scripts\*.xpm
    
	${If} $WANTS_CG_UPGRADE == "Yes"
		; Copy Cg DLLs.
		; Note that this will overwrite existing files. We back them up
		; first, if the backup don't already exist.
		IfFileExists "$MAYA_2008_DIR\bin\Cg.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2008_DIR\bin\Cg.dll" "$MAYA_2008_DIR\bin\Cg.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_2008_DIR\bin\CgGL.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2008_DIR\bin\CgGL.dll" "$MAYA_2008_DIR\bin\CgGL.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_2008_DIR\bin\glut32.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_2008_DIR\bin\glut32.dll" "$MAYA_2008_DIR\bin\glut32.backup_before_ColladaMaya.dll"

		SetOutPath $MAYA_2008_DIR\bin
	    ${If} $WINDOWS_SYSTEM == "32bit"
			File ..\..\..\External\Cg\bin\cg.dll
			File ..\..\..\External\Cg\bin\cgGL.dll
			File ..\..\..\External\Cg\bin\glut32.dll
	    ${Endif}
	    ${If} $WINDOWS_SYSTEM == "64bit"
			File ..\..\..\External\Cg\bin.x64\cg.dll
			File ..\..\..\External\Cg\bin.x64\cgGL.dll
			File ..\..\..\External\Cg\bin.x64\glut32.dll
	    ${Endif}
	${Endif}
    
    End2008:
    
SectionEnd

; The stuff to install
Section "Plug-in for Maya 8.5"

    ; Skip this section if Maya 8.5 is not installed
    StrCmp $MAYA_85_DIR "" End85

    ; Copy plug-in itself
    SetOutPath $MAYA_85_DIR\bin\plug-ins
    ${If} $WINDOWS_SYSTEM == "32bit"
	    File "..\..\Output\Maya 8.5 Win32\COLLADA.mll"
    ${Endif}
    ${If} $WINDOWS_SYSTEM == "64bit"
	    File "..\..\Output\Maya 8.5 x64\COLLADA.mll"
    ${Endif}

    
    ; Copy regular scripts (all mel files except AE templates)
    SetOutPath $MAYA_85_DIR\scripts\others
	Delete $MAYA_85_DIR\scripts\others\AEcolladafxPassesTemplate.mel
	Delete $MAYA_85_DIR\scripts\others\AEcolladafxShaderTemplate.mel
    File /r /x AE*.mel ..\..\scripts\*.mel
    
    ; Copy AE templates scripts
    SetOutPath $MAYA_85_DIR\scripts\AETemplates
    File ..\..\scripts\AE*.mel
    
    ; Copy XPM icons
    SetOutPath $MAYA_85_DIR\icons
    File ..\..\scripts\*.xpm
    
	${If} $WANTS_CG_UPGRADE == "Yes"
		; Copy Cg DLLs.
		; Note that this will overwrite existing files. We back them up
		; first, if the backup don't already exist.
		IfFileExists "$MAYA_85_DIR\bin\Cg.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_85_DIR\bin\Cg.dll" "$MAYA_85_DIR\bin\Cg.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_85_DIR\bin\CgGL.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_85_DIR\bin\CgGL.dll" "$MAYA_85_DIR\bin\CgGL.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_85_DIR\bin\glut32.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_85_DIR\bin\glut32.dll" "$MAYA_85_DIR\bin\glut32.backup_before_ColladaMaya.dll"

		SetOutPath $MAYA_85_DIR\bin
			    ${If} $WINDOWS_SYSTEM == "32bit"
			File ..\..\..\External\Cg\bin\cg.dll
			File ..\..\..\External\Cg\bin\cgGL.dll
			File ..\..\..\External\Cg\bin\glut32.dll
	    ${Endif}
	    ${If} $WINDOWS_SYSTEM == "64bit"
			File ..\..\..\External\Cg\bin.x64\cg.dll
			File ..\..\..\External\Cg\bin.x64\cgGL.dll
			File ..\..\..\External\Cg\bin.x64\glut32.dll
	    ${Endif}

	${Endif}
    
    End85:
    
SectionEnd

; The stuff to install
Section "Plug-in for Maya 8.0"

    ; Skip this section if Maya 8.0 is not installed
    StrCmp $MAYA_80_DIR "" End80
    
    ; Copy plug-in itself
    SetOutPath $MAYA_80_DIR\bin\plug-ins
    File "..\..\Output\Maya 8.0 Win32\COLLADA.mll"
    
    ; Copy regular scripts (all mel files except AE templates)
    SetOutPath $MAYA_80_DIR\scripts\others
	Delete $MAYA_80_DIR\scripts\others\AEcolladafxPassesTemplate.mel
	Delete $MAYA_80_DIR\scripts\others\AEcolladafxShaderTemplate.mel
    File /r /x AE*.mel ..\..\scripts\*.mel
    
    ; Copy AE templates scripts
    SetOutPath $MAYA_80_DIR\scripts\AETemplates
    File ..\..\scripts\AE*.mel
    
    ; Copy XPM icons
    SetOutPath $MAYA_80_DIR\icons
    File ..\..\scripts\*.xpm
    
	${If} $WANTS_CG_UPGRADE == "Yes"
		; Copy Cg DLLs.
		; Note that this will overwrite existing files. We back them up
		; first, if the backup don't already exist.
		IfFileExists "$MAYA_80_DIR\bin\Cg.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_80_DIR\bin\Cg.dll" "$MAYA_80_DIR\bin\Cg.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_80_DIR\bin\CgGL.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_80_DIR\bin\CgGL.dll" "$MAYA_80_DIR\bin\CgGL.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_80_DIR\bin\glut32.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_80_DIR\bin\glut32.dll" "$MAYA_80_DIR\bin\glut32.backup_before_ColladaMaya.dll"

		SetOutPath $MAYA_80_DIR\bin
		File ..\..\..\External\Cg\bin\cg.dll
		File ..\..\..\External\Cg\bin\cgGL.dll
		File ..\..\..\External\Cg\bin\glut32.dll
	${EndIf}
    
    End80:

SectionEnd

Section "Plug-in for Maya 7.0"

    ; Skip this section if Maya 7.0 is not installed
    StrCmp $MAYA_70_DIR "" End70
        
    ; Copy plug-in itself
    SetOutPath $MAYA_70_DIR\bin\plug-ins
    File "..\..\Output\Maya 7.0 Win32\COLLADA.mll"
    
    ; Copy regular scripts (all mel files except AE templates)
    SetOutPath $MAYA_70_DIR\scripts\others
	Delete $MAYA_70_DIR\scripts\others\AEcolladafxPassesTemplate.mel
	Delete $MAYA_70_DIR\scripts\others\AEcolladafxShaderTemplate.mel
    File /r /x AE*.mel ..\..\scripts\*.mel
    
    ; Copy AE templates scripts
    SetOutPath $MAYA_70_DIR\scripts\AETemplates
    File ..\..\scripts\AE*.mel
    
    ; Copy XPM icons
    SetOutPath $MAYA_70_DIR\icons
    File ..\..\scripts\*.xpm
    
	${If} $WANTS_CG_UPGRADE == "Yes"
		; Copy Cg DLLs.
		; Note that this will overwrite existing files. We back them up
		; first, if the backup don't already exist.
		IfFileExists "$MAYA_70_DIR\bin\Cg.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_70_DIR\bin\Cg.dll" "$MAYA_70_DIR\bin\Cg.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_70_DIR\bin\CgGL.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_70_DIR\bin\CgGL.dll" "$MAYA_70_DIR\bin\CgGL.backup_before_ColladaMaya.dll"
	
		IfFileExists "$MAYA_70_DIR\bin\glut32.backup_before_ColladaMaya.dll" +2 0
			CopyFiles "$MAYA_70_DIR\bin\glut32.dll" "$MAYA_70_DIR\bin\glut32.backup_before_ColladaMaya.dll"

		SetOutPath $MAYA_70_DIR\bin
		File ..\..\..\External\Cg\bin\cg.dll
		File ..\..\..\External\Cg\bin\cgGL.dll
		File ..\..\..\External\Cg\bin\glut32.dll
	${EndIf}
    
    End70:

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Feeling Software\ColladaMaya"
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMaya\Online ColladaMaya Documentation.lnk" "http://www.feelingsoftware.com/content/view/55/72/lang,en/" "" "http://www.feelingsoftware.com/content/view/55/72/lang,en/" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMaya\Feeling Software.com.lnk" "http://www.feelingsoftware.com" "" "http://www.feelingsoftware.com" 0  
  CreateShortCut "$SMPROGRAMS\Feeling Software\ColladaMaya\Uninstall.lnk" "$INSTDIR\uninstallColladaMaya.exe" "" "$INSTDIR\uninstallColladaMaya.exe" 0  
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  SetShellVarContext all ; Uninstall the shortcuts in all users's start-menus.

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Feeling Software\ColladaMaya"
  DeleteRegKey HKLM SOFTWARE\FeelingSoftware\ColladaMaya

  ; Remove files and uninstaller
  Delete $INSTDIR\uninstallColladaMaya.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Feeling Software\ColladaMaya\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Feeling Software\ColladaMaya"
  RMDir "$SMPROGRAMS\Feeling Software" ; in case there is no other Feeling Software product installed.
  RMDir /r "$INSTDIR"

SectionEnd
