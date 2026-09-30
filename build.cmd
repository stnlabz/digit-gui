@echo off
setlocal

if not exist build mkdir build

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe was not found. Run from an MSVC Developer Command Prompt or initialize the MSVC build environment.
    exit /b 1
)

echo Building Digit GUI...
cl /nologo /std:c11 /W4 /O2 /D_CRT_SECURE_NO_WARNINGS src\main.c /Fe:build\digit-gui.exe /link winhttp.lib user32.lib gdi32.lib
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
