@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."

rem ============================================================
rem  Build all demo projects under demo\*\MDK-ARM\*.uvprojx
rem  Usage: scripts\build_all.bat [--fast]
rem ============================================================

set "BUILD_MODE=-r"
if /i "%~1"=="--fast" set "BUILD_MODE=-b"

set "UV4="
if exist "D:\K5\UV4\UV4.exe" set "UV4=D:\K5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles%\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe"
if not defined UV4 (
  echo ERROR: UV4.exe not found.
  exit /b 1
)

set /a TOTAL=0
set /a PASSED=0
set /a FAILED=0
set "TMP_LOG=%TEMP%\stm32_hal_build_%RANDOM%.log"

for /d %%d in ("demo\*") do (
  for %%p in ("%%d\MDK-ARM\*.uvprojx") do (
    set /a TOTAL+=1
    set "PROJ=%%~fp"
    set "TGT="
    for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "try{$p=[xml](Get-Content -Raw '%%~fp'); @($p.Project.Targets.Target.TargetName)[0]}catch{''}"`) do set "TGT=%%t"

    if not defined TGT (
      echo [FAIL] %%d - cannot read TargetName from %%~nxp
      set /a FAILED+=1
    ) else (
      if exist "%TMP_LOG%" del /q "%TMP_LOG%" >nul 2>&1
      "%UV4%" %BUILD_MODE% "!PROJ!" -t "!TGT!" -o "%TMP_LOG%"
      findstr /c:" 0 Error(s), 0 Warning(s)" "%TMP_LOG%" >nul 2>&1
      if errorlevel 1 (
        echo [FAIL] %%d ^(!TGT!^)
        if exist "%TMP_LOG%" type "%TMP_LOG%"
        set /a FAILED+=1
      ) else (
        set /a PASSED+=1
        for /f "delims=" %%s in ('findstr /c:"Program Size:" "%TMP_LOG%"') do (
          echo [PASS] %%d ^(!TGT!^) - %%s
        )
      )
    )
  )
)

if exist "%TMP_LOG%" del /q "%TMP_LOG%" >nul 2>&1
echo.
echo Summary: !PASSED!/!TOTAL! passed, !FAILED! failed.
if !FAILED! gtr 0 exit /b 1
exit /b 0
