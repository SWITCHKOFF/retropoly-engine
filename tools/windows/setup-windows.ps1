# ============================================================================
# setup-windows.ps1 — Спринт 0 §3.1 (docs/plans/sprint_00.md).
#
# Единая автоматизация установки базового Windows-11-инструментария:
#   1. Git for Windows 2.40+        («Checkout as-is, commit Unix-style»)
#   2. CMake 3.25+                  (Add to system PATH)
#   3. Ninja 1.11+                  (C:\Tools\ninja в machine-PATH)
#   4. VS 2022 Build Tools          (MSVC v143 + Windows 11 SDK 22621.0+)
#   5. MSYS2 → C:\msys64            (base-devel + git)
#   6. Cursor IDE                   (+ 7 расширений)
#   7. Эмуляторы: DuckStation (manual) + PCSX-Redux
#
# Требования: Windows 11 x64, PowerShell 5.1+ (администратор), интернет.
# Ступени идемпотентны: повторный прогон пропускает установленный софт.
# Каждая ступень завершает ПРОВЕРКОЙ (критерий приёмки §3.1); итог — сводка
# [OK]/[FAIL] в конце прогона (чек-боксы спринта закрываются по сводке).
# ============================================================================
#Requires -RunAsAdministrator
$ErrorActionPreference = "Stop"

Write-Host "=== RetroPolyEngine: Windows-инструментарий §3.1 ===" -ForegroundColor Cyan

function Test-Installed
{
    param([string]$Name)
    return ($null -ne (Get-Command $Name -ErrorAction SilentlyContinue))
}

function Get-ToolVersion
{
    param([scriptblock]$Probe)
    $out = & $Probe 2>$null | Select-Object -First 1
    if (-not $out) { return "NOT-INSTALLED" }
    return [string]$out
}

function Add-MachinePath
{
    param([string]$Segment)
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if ($machine -notlike "*$Segment*") {
        [Environment]::SetEnvironmentVariable("Path", "$machine;$Segment",
            "Machine")
    }
    if ($env:Path -notlike "*$Segment*") {
        $env:Path += ";$Segment"
    }
}

function Invoke-Winget
{
    param([string]$PackageId)
    if (Test-Installed winget) {
        Write-Host ">> winget install $PackageId ..."
        winget install --id $PackageId -e --silent `
            --accept-package-agreements --accept-source-agreements
        return $LASTEXITCODE -eq 0
    }
    Write-Host "[FAIL] winget недоступен (App Installer из Microsoft Store)" `
        -ForegroundColor Red
    return $false
}

# --- 1. Git for Windows (2.40+; «Checkout as-is, commit Unix-style») ---------
if (-not (Test-Installed git)) {
    Invoke-Winget "Git.Git" | Out-Null
    Add-MachinePath "C:\Program Files\Git\cmd"
}
Write-Host "Git: $(Get-ToolVersion { git --version })"
# Конфигурация line endings по чек-листу §3.1 (as-is checkout, Unix commit).
git config --global core.autocrlf false
Write-Host "Git: core.autocrlf=false (checkout as-is / commit Unix-style)"

# --- 2. CMake (3.25+; system PATH) -------------------------------------------
if (-not (Test-Installed cmake)) {
    Invoke-Winget "Kitware.CMake" | Out-Null
    Add-MachinePath "C:\Program Files\CMake\bin"
}
Write-Host "CMake: $(Get-ToolVersion { cmake --version })"

# --- 3. Ninja (1.11+; C:\Tools\ninja в machine-PATH) --------------------------
if (-not (Test-Installed ninja)) {
    Invoke-Winget "Ninja-build.Ninja" | Out-Null
    $ninjaExe = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" `
        -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue | `
        Select-Object -First 1
    if ($ninjaExe) {
        New-Item -ItemType Directory -Force "C:\Tools\ninja" | Out-Null
        Copy-Item $ninjaExe.FullName "C:\Tools\ninja\ninja.exe" -Force
        Add-MachinePath "C:\Tools\ninja"
    }
}
Write-Host "Ninja: $(Get-ToolVersion { ninja --version })"

# --- 4. VS 2022 Build Tools (MSVC v143 + Win11 SDK 22621.0+) ------------------
$vsRoot = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
if (-not (Test-Installed cl)) {
    Invoke-Winget "Microsoft.VisualStudio.2022.BuildTools" | Out-Null
    & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" `
        modify --installPath $vsRoot --norestart --quiet `
        --add Microsoft.VisualStudio.Workload.VCTools `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.Windows11SDK.22621 | Out-Null
}
# cl доступен ТОЛЬКО в Developer-окружении — проверяем программно (vsdevcmd):
$vsDevCmd = Join-Path $vsRoot "Common7\Tools\vsdevcmd.bat"
if (Test-Path $vsDevCmd) {
    $clLine = cmd /c "`"$vsDevCmd`" >nul 2>&1 && cl 2>&1" | Select-Object -First 1
    Write-Host "MSVC cl: $clLine"
}
else {
    Write-Host "MSVC cl: Developer-окружение не найдено (см. сводку)" `
        -ForegroundColor Yellow
}

# --- 5. MSYS2 → C:\msys64 (host shell; НЕ источник PS1-компилятора) -----------
if (Test-Path "C:\msys64\usr\bin\pacman.exe") {
    & "C:\msys64\usr\bin\bash.exe" -lc "pacman -Syu --noconfirm"
    & "C:\msys64\usr\bin\bash.exe" -lc `
        "pacman -S --needed --noconfirm base-devel git"
    Write-Host "MSYS2: base-devel git — установлены"
}
else {
    Invoke-Winget "MSYS2.MSYS2" | Out-Null
    Write-Host "MSYS2: установка выполнена — запустите pacman-ступень скрипта" `
        "повторно из новой консоли (PATH от winget)" -ForegroundColor Yellow
}

# --- 6. Cursor IDE + 7 расширений ---------------------------------------------
Invoke-Winget "Anysphere.Cursor" | Out-Null
$cursorCli = "$env:LOCALAPPDATA\Programs\cursor\resources\app\bin\cursor"
Write-Host "Cursor: $(Get-ToolVersion { & $cursorCli --version })"
if (Test-Path $cursorCli) {
    foreach ($ext in "ms-vscode.cpptools", "ms-vscode.cmake-tools",
                     "llvm-vs-code-extensions.vscode-clangd",
                     "eamodio.gitlens", "usernamehw.errorlens",
                     "vadimcn.vscode-lldb", "ms-vscode.cpptools-extension-pack") {
        & $cursorCli --install-extension $ext --force | Out-Null
        Write-Host "Cursor ext: $ext — OK"
    }
}
else {
    Write-Host "[FAIL] cursor CLI не найден — 7 расширений ставятся вручную" `
        -ForegroundColor Yellow
}

# --- 7. Эмуляторы --------------------------------------------------------------
# PCSX-Redux — через winget; DuckStation preview — вручную (канал дистрибутива
# меняется редко/постепенно; см. чек-лист §3.1, каталог C:\emulators\DuckStation).
Invoke-Winget "RetroAchievements.PCSX-Redux" | Out-Null
if (-not (Test-Path "C:\emulators\DuckStation")) {
    Write-Host "DuckStation: скачайте preview с duckstation.org в" `
        "C:\emulators\DuckStation\" -ForegroundColor Yellow
}

# --- Итоговая сводка (критерии приёмки §3.1) -----------------------------------
Write-Host ""
Write-Host "=== Сводка §3.1 ===" -ForegroundColor Cyan
$git_ok = ($null -ne (git --version 2>$null))
$cmake_ok = ($null -ne (cmake --version 2>$null))
$ninja_ok = ($null -ne (ninja --version 2>$null))
$cl_ok = Test-Path $vsDevCmd
$msys_ok = Test-Path "C:\msys64\usr\bin\pacman.exe"
$cursor_ok = (Test-Path $cursorCli) -and ($null -ne (& $cursorCli --list-extensions 2>$null))
$duck_ok = Test-Path "C:\emulators\DuckStation"
$redux_ok = ($null -ne (Get-Command pcsx-redux -ErrorAction SilentlyContinue)) -or `
    (Test-Path "$env:LOCALAPPDATA\PCSX-Redux\pcsx-redux.exe")
foreach ($entry in @(
        @{ Name = "Git for Windows 2.40+ (as-is/Unix)"; Ok = $git_ok },
        @{ Name = "CMake 3.25+ (PATH)";                Ok = $cmake_ok },
        @{ Name = "Ninja 1.11+ (C:\Tools)";            Ok = $ninja_ok },
        @{ Name = "VS Build Tools: cl /? работает";    Ok = $cl_ok },
        @{ Name = "MSYS2 base-devel + git";            Ok = $msys_ok },
        @{ Name = "Cursor + 7 расширений";             Ok = $cursor_ok },
        @{ Name = "DuckStation → C:\emulators";        Ok = $duck_ok },
        @{ Name = "PCSX-Redux";                        Ok = $redux_ok })) {
    $mark = if ($entry["Ok"]) { "[OK]" } else { "[FAIL]" }
    Write-Host "$mark $($entry['Name'])"
}
Write-Host ""
Write-Host "Дальше: закрыть чек-боксы docs/plans/sprint_00.md §3.1 по сводке;" `
    "перезапустить терминал (PATH)." -ForegroundColor Green
