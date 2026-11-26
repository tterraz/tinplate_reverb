; Tinplate Reverb - Windows installer (NSIS 3)
; Copyright (C) 2026 Tinplate authors - SPDX-License-Identifier: GPL-3.0-or-later
; Build: makensis installer/tinplate.nsi  (from ../build.sh, after the Windows build)

Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define NAME     "Tinplate Reverb"
!define VERSION  "1.1.0"
!define BUNDLE   "tinplate_reverb"
!define MANUAL   "Tinplate-Reverb-manuel.pdf"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\TinplateReverb"

Name "${NAME} ${VERSION}"
OutFile "../bin/Tinplate-Reverb-${VERSION}-setup.exe"
RequestExecutionLevel admin
InstallDir "$COMMONFILES64\VST3"
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "../COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "${NAME} needs 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
FunctionEnd

Function un.onInit
    SetRegView 64
FunctionEnd

Section "VST3" SecVST3
    SectionIn RO
    SetOutPath "$COMMONFILES64\VST3"
    File /r "../bin/windows/${BUNDLE}.vst3"
    SetOutPath "$PROGRAMFILES64\Tinplate"
    File "../COPYING"
    WriteUninstaller "$PROGRAMFILES64\Tinplate\uninstall.exe"
    WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${NAME}"
    WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "Tinplate"
    WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$PROGRAMFILES64\Tinplate\uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "CLAP" SecCLAP
    SetOutPath "$COMMONFILES64\CLAP"
    File "../bin/windows/${BUNDLE}.clap"
SectionEnd

; user manual (French, PDF) next to the licence, with a Start menu shortcut for all users
Section "Manual (PDF)" SecManual
    SetShellVarContext all
    SetOutPath "$PROGRAMFILES64\Tinplate"
    File "/oname=${MANUAL}" "../doc/manuel.pdf"
    CreateDirectory "$SMPROGRAMS\${NAME}"
    CreateShortcut "$SMPROGRAMS\${NAME}\${NAME} - manuel.lnk" "$PROGRAMFILES64\Tinplate\${MANUAL}"
SectionEnd

Section "Uninstall"
    SetShellVarContext all
    Delete "$SMPROGRAMS\${NAME}\${NAME} - manuel.lnk"
    RMDir "$SMPROGRAMS\${NAME}"
    Delete "$PROGRAMFILES64\Tinplate\${MANUAL}"
    RMDir /r "$COMMONFILES64\VST3\${BUNDLE}.vst3"
    Delete "$COMMONFILES64\CLAP\${BUNDLE}.clap"
    Delete "$PROGRAMFILES64\Tinplate\COPYING"
    Delete "$PROGRAMFILES64\Tinplate\uninstall.exe"
    RMDir "$PROGRAMFILES64\Tinplate"
    DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
