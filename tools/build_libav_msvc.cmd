@echo off
rem Builds build\libav\windows-msvc (/MD, the dev build) and windows-msvc-mt (/MT, the release
rem build) for Visual Studio (tools/build_libav.sh windows-msvc windows-msvc-mt).
rem Needs VS 2022 Build Tools and an MSYS2 bash with make (MOON_MSYS_BASH, else devkitPro's or C:\msys64).
setlocal
set "VSDEV=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
call "%VSDEV%" -arch=x64 -host_arch=x64 >nul || exit /b 1
if not defined MOON_MSYS_BASH if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "MOON_MSYS_BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not defined MOON_MSYS_BASH set "MOON_MSYS_BASH=C:\msys64\usr\bin\bash.exe"
set "MOON_ROOT=%~dp0.."
"%MOON_MSYS_BASH%" --noprofile --norc -c "export PATH=\"$(/usr/bin/cygpath -u \"$VCToolsInstallDir\")bin/Hostx64/x64:/usr/bin:$PATH\"; cd \"$(/usr/bin/cygpath -u \"$MOON_ROOT\")\" && sh tools/build_libav.sh windows-msvc windows-msvc-mt"
