SetCompressor /SOLID LZMA
RequestExecutionLevel admin
OutFile scscinst.exe
InstallDir "$PROGRAMFILES\ScamScan"
Name "ScamScan"
BrandingText "${__TIMESTAMP__}"

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

LicenseData LICENSE.txt

Section "!ScamScan"
	SectionIn RO
	
	CreateDirectory $INSTDIR
	SetOutPath $INSTDIR
	
	WriteRegStr HKLM "SOFTWARE\Electroduck\ScamScan" "InstallDir" "$INSTDIR"
	
	File scamscan.exe
	File LIBCURL.DLL
	File pcre3.dll
	File deltemp.bat
	File scscrun.bat
	File delcook.bat
	File /oname=list.txt list_4install.txt
	
	FileOpen $0 "$INSTDIR\cookies.dat" w
	FileClose $0
	
	CreateDirectory "$INSTDIR\tempdl"
	
	AccessControl::GrantOnFile "$INSTDIR\list.txt" "(S-1-5-32-545)" "FullAccess"
	AccessControl::GrantOnFile "$INSTDIR\tempdl" "(S-1-5-32-545)" "FullAccess"
	AccessControl::GrantOnFile "$INSTDIR\cookies.dat" "(S-1-5-32-545)" "FullAccess"
	
	WriteUninstaller uninst.exe
SectionEnd

Section "Start Menu Icons"
	StrCpy $0 "$SMPROGRAMS\ScamScan"
	WriteRegStr HKLM "SOFTWARE\Electroduck\ScamScan" "StartMenuDir" "$0"
	SetOutPath $INSTDIR ; correct working directory for link launched programs
	CreateDirectory "$0"
	CreateShortCut "$0\Run ScamScan.lnk" "cmd.exe" '/c "$INSTDIR\scscrun.bat"' "shell32.dll" 22
	CreateShortCut "$0\Edit Website List.lnk" "notepad.exe" '"$INSTDIR\list.txt"' "write.exe"
	CreateShortCut "$0\Purge Temp Files.lnk" "cmd.exe" '/c "$INSTDIR\deltemp.bat"' "shell32.dll" 31
	CreateShortCut "$0\Clear Cookies.lnk" "cmd.exe" '/c "$INSTDIR\delcook.bat"' "shell32.dll" 31
	CreateShortCut "$0\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section "Desktop Icons"
	SetOutPath $INSTDIR ; correct working directory for link launched programs
	WriteRegDWORD HKLM "SOFTWARE\Electroduck\ScamScan" "DesktopIcons" 1
	CreateShortCut "$DESKTOP\Run ScamScan.lnk" "cmd.exe" '/c "$INSTDIR\scscrun.bat"' "shell32.dll" 22
	CreateShortCut "$DESKTOP\Edit ScamScan Website List.lnk" "notepad.exe" '"$INSTDIR\list.txt"' "write.exe"
SectionEnd

Section "Uninstall"
	ReadRegStr $0 HKLM "SOFTWARE\Electroduck\ScamScan" "InstallDir"
	RMDir /r $0
	
	ReadRegStr $0 HKLM "SOFTWARE\Electroduck\ScamScan" "StartMenuDir"
	StrCmp $0 "" noSMIcons
		RMDir /r $0
	noSMIcons:
	
	ReadRegDWORD $0 HKLM "SOFTWARE\Electroduck\ScamScan" "DesktopIcons"
	IntCmp $0 0 noDtopIcons
		Delete "$DESKTOP\Run ScamScan.lnk"
		Delete "$DESKTOP\Edit ScamScan Website List.lnk"
	noDtopIcons:
	
	DeleteRegKey HKLM "SOFTWARE\Electroduck\ScamScan"
SectionEnd
