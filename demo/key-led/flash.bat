@echo off
setlocal
cd /d "%~dp0"

rem ============================================================
rem  One-click: build -> flash -> reset
rem  Double-click this file. No Keil "Reset and Run" needed.
rem ============================================================

echo [1/3] Locating project...
set "PROJ="
for %%f in ("MDK-ARM\*.uvprojx") do set "PROJ=%%~ff"
if not defined PROJ (
  echo   ERROR: no .uvprojx found under MDK-ARM\
  goto :fail
)

set "TGT="
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "try{$p=[xml](Get-Content -Raw '%PROJ%'); @($p.Project.Targets.Target.TargetName)[0]}catch{''}"`) do set "TGT=%%t"
if not defined TGT (
  echo   ERROR: cannot read TargetName from the project file
  goto :fail
)
echo   project : %PROJ%
echo   target  : %TGT%

set "UV4="
if exist "D:\K5\UV4\UV4.exe" set "UV4=D:\K5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles%\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe"
if not defined UV4 (
  echo   ERROR: UV4.exe not found. Edit this file and set the UV4 path.
  goto :fail
)

echo [2/3] Building...
"%UV4%" -b "%PROJ%" -t "%TGT%" -o "%~dp0build.log"
findstr /c:" 0 Error(s)" "%~dp0build.log" >nul
if errorlevel 1 (
  echo   ERROR: build failed. See build.log
  echo   Tip: close Keil first if it has this project open.
  goto :fail
)

set "HEX=MDK-ARM\%TGT%\%TGT%.hex"
if not exist "%HEX%" (
  echo   ERROR: hex not found: %HEX%
  goto :fail
)

set "CLI="
for /d %%d in ("%LOCALAPPDATA%\stm32cube\bundles\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI if exist "%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "CLI=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if not defined CLI (
  echo   ERROR: STM32_Programmer_CLI.exe not found.
  echo   Install STM32CubeProgrammer, or edit this file and set the CLI path.
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
pause
exit /b 0

:fail
echo.
echo   ABORTED
echo.
pause
exit /b 1
