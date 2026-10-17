@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed - check the Visual Studio installation path.
    exit /b 1
)

cd /d "%~dp0"

rem 平台宏统一在这里定义，源码里不再重复 #define
set PLATFORM_DEFINES=/D_WIN32_WINNT=0x0A00 /DUNICODE /D_UNICODE
set CFLAGS=/nologo /W4 /O1 /MD /utf-8
set LIBS=advapi32.lib shell32.lib user32.lib comctl32.lib shlwapi.lib
set SOURCES=main.c ui.c security.c process.c fileops.c

echo === Compiling resources ===
rc /nologo /utf-8 power.rc
if errorlevel 1 (
    echo [ERROR] resource compilation failed.
    exit /b 1
)

echo === Compiling and linking ===
cl %CFLAGS% %PLATFORM_DEFINES% %SOURCES% power.res /Fe"Power.exe" /link %LIBS% /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:EMBED
if errorlevel 1 (
    echo [ERROR] build failed.
    exit /b 1
)

where upx >nul 2>&1
if errorlevel 1 (
    echo [WARN] upx not found on PATH - skipping compression.
) else (
    upx --ultra-brute Power.exe
    if errorlevel 1 echo [WARN] upx failed - the uncompressed binary is still usable.
)

echo Build succeeded.

echo clean
cd /d "%~dp0"

echo Cleaning build artifacts...
del /f /q *.obj *.res 2>nul

echo Done.
exit /b 0
