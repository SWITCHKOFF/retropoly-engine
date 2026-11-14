@echo off
rem SPDX-License-Identifier: MIT
rem ============================================================================
rem RetroPolyEngine — scripts/run_n64.bat (S39–40, ADR-014)
rem
rem Запуск boot-demo в pinned gopher64 1.2.1 (Windows x86_64).
rem Эмулятор/ROM не вендорятся; BIOS не нужен.
rem
rem   set GOPHER64_BIN=C:\tools\gopher64.exe
rem   scripts\run_n64.bat [ROM.z64]
rem ============================================================================
setlocal

set "REPO_ROOT=%~dp0.."
if "%~1"=="" (
    set "ROM=%REPO_ROOT%\build\n64-release\src\hal\n64\hello_n64.z64"
) else (
    set "ROM=%~1"
)
if "%GOPHER64_BIN%"=="" set "GOPHER64_BIN=gopher64.exe"

if not exist "%ROM%" (
    echo run_n64: ROM не найден: %ROM%
    echo   Соберите: cmake --preset n64-release ^&^& cmake --build --preset n64-release
    exit /b 1
)

"%GOPHER64_BIN%" --disable-expansion-pak false --overclock false "%ROM%"
exit /b %ERRORLEVEL%
