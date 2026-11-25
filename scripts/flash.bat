@echo off
setlocal
cd /d "%~dp0.."

rem ============================================================
rem  Build -> Flash -> Verify -> Reset for any demo
rem  Usage: scripts\flash.bat <demo-name> [--no-pause]
rem  Example: scripts\flash.bat adc
rem ============================================================

set "DEMO=%~1"
if not defined DEMO (
  echo Usage: scripts\flash.bat ^<demo-name^> [--no-pause]
  echo Available demos:
  for /d %%d in ("demo\*") do echo   - %%~nxd
  if /i not "%~2"=="--no-pause" pause
  exit /b 1
)

set "DEMO_DIR=%CD%\demo\%DEMO%"
if not exist "%DEMO_DIR%\MDK-ARM" (
  echo ERROR: demo not found: demo\%DEMO%
  if /i not "%~2"=="--no-pause" pause
  exit /b 1
)

echo [1/3] Locating project...
set "PROJ="
for %%f in ("%DEMO_DIR%\MDK-ARM\*.uvprojx") do set "PROJ=%%~ff"
if not defined PROJ (
  echo   ERROR: no .uvprojx found under demo\%DEMO%\MDK-ARM\
  goto :fail
)

set "TGT="
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "try{$p=[xml](Get-Content -Raw '%PROJ%'); @($p.Project.Targets.Target.TargetName)[0]}catch{''}"`) do set "TGT=%%t"
if not defined TGT (
  echo   ERROR: cannot read TargetName from %PROJ%
  goto :fail
)
echo   demo    : %DEMO%
echo   project : %PROJ%
echo   target  : %TGT%

set "UV4="
if exist "D:\K5\UV4\UV4.exe" set "UV4=D:\K5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles%\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe"
if not defined UV4 (
  echo   ERROR: UV4.exe not found.
  goto :fail
)

echo [2/3] Building...
set "TMP_LOG=%TEMP%\stm32_flash_%RANDOM%.log"
"%UV4%" -b "%PROJ%" -t "%TGT%" -o "%TMP_LOG%"
findstr /c:" 0 Error(s)" "%TMP_LOG%" >nul 2>&1
if errorlevel 1 (
  echo   ERROR: build failed:
  if exist "%TMP_LOG%" type "%TMP_LOG%"
  if exist "%TMP_LOG%" del /q "%TMP_LOG%" >nul 2>&1
  goto :fail
)
if exist "%TMP_LOG%" del /q "%TMP_LOG%" >nul 2>&1

set "HEX=%DEMO_DIR%\MDK-ARM\%TGT%\%TGT%.hex"
if not exist "%HEX%" (
  echo   ERROR: hex not found: %HEX%
  goto :fail
)

set "CLI="
for /d %%d in ("%LOCALAPPDATA%\stm32cube\bundles\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI if exist "%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "CLI=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if not defined CLI (
  echo   ERROR: STM32_Programmer_CLI.exe not found.
  goto :fail
)

echo [3/3] Flashing (write + verify + reset)...
"%CLI%" -c port=SWD -w "%HEX%" -v -rst
if errorlevel 1 (
  echo   ERROR: flashing failed. Check ST-Link wiring.
  goto :fail
)

echo.
echo   DONE - firmware written, MCU reset and running.
echo.
if /i not "%~2"=="--no-pause" pause
exit /b 0

:fail
echo.
echo   ABORTED
echo.
if /i not "%~2"=="--no-pause" pause
exit /b 1
