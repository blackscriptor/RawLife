@echo off
REM Build script for RawLife. Run from a "Developer Command Prompt for VS".
REM Produces build\rawlife.exe

if not exist build mkdir build

cl.exe /nologo /W4 /WX /O2 /Fe:build\rawlife.exe ^
    src\platform\win32_main.c ^
    src\sim\arena.c ^
    /I src ^
    /link user32.lib gdi32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b 1
)

echo Build succeeded: build\rawlife.exe
