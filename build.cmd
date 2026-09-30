@echo off
setlocal

if /I "%~1"=="clean" goto clean
if not "%~1"=="" (
    echo Usage: build.cmd [clean]
    exit /b 2
)

if not exist build mkdir build

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe was not found. Run from an MSVC Developer Command Prompt or initialize the MSVC build environment.
    exit /b 1
)

echo Building Digit GUI...
cl /nologo /std:c11 /W4 /O2 /D_CRT_SECURE_NO_WARNINGS src\main.c /Fe:build\digit-gui.exe /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup winhttp.lib user32.lib gdi32.lib
if errorlevel 1 (
    echo BUILD FAILED.
    exit /b 1
)

copy /Y config\digit.conf build\digit.conf >nul
if errorlevel 1 (
    echo CONFIG COPY FAILED.
    exit /b 1
)

echo Running required self-test...
build\digit-gui.exe --self-test
if errorlevel 1 (
    echo SELF-TEST FAILED.
    exit /b 1
)

echo.
echo BUILD GREEN: build\digit-gui.exe
exit /b 0

:clean
echo Cleaning Digit GUI build...
if exist build rmdir /S /Q build
if exist build (
    echo CLEAN FAILED.
    exit /b 1
)
echo CLEAN GREEN.
exit /b 0
