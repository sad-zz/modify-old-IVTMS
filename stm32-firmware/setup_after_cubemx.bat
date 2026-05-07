@echo off
rem ─────────────────────────────────────────────────────────────────────
rem  setup_after_cubemx.bat
rem  Windows version of setup_after_cubemx.sh
rem  Run this AFTER pressing "Generate Code" in STM32CubeIDE/CubeMX.
rem ─────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ──────────────────────────────────────────────
echo  RATCX1-STM32 – Post-CubeMX Setup
echo  Working directory: %CD%
echo ──────────────────────────────────────────────

if not exist "Core\Src\system_stm32f1xx.c" (
    echo.
    echo  [ERROR] CubeMX project structure not found.
    echo  Open RATCX1-STM32.ioc in CubeIDE and click 'Generate Code' first,
    echo  then run this script.
    echo.
    pause
    exit /b 1
)

echo.
echo Step 1: Backing up CubeMX-generated files...
if not exist ".cubemx_backup" mkdir ".cubemx_backup"
for %%f in (Core\Src\main.c Core\Src\stm32f1xx_it.c Core\Inc\main.h Core\Inc\stm32f1xx_it.h) do (
    if exist "%%f" if not exist ".cubemx_backup\%%~nxf" (
        copy "%%f" ".cubemx_backup\%%~nxf" >nul
        echo   backed up %%f
    )
)

echo.
echo Step 2: Re-applying RATCX1 application files...
set GOLDEN=.ratcx1_originals
if not exist "%GOLDEN%" (
    mkdir "%GOLDEN%\Core\Src" 2>nul
    mkdir "%GOLDEN%\Core\Inc" 2>nul
    copy Core\Src\main.c          "%GOLDEN%\Core\Src\"          >nul 2>&1
    copy Core\Src\stm32f1xx_it.c  "%GOLDEN%\Core\Src\"          >nul 2>&1
    copy Core\Inc\main.h          "%GOLDEN%\Core\Inc\"          >nul 2>&1
    copy Core\Inc\stm32f1xx_it.h  "%GOLDEN%\Core\Inc\"          >nul 2>&1
    echo   saved golden copies to %GOLDEN%\
)

copy /y "%GOLDEN%\Core\Src\main.c"          Core\Src\main.c          >nul
copy /y "%GOLDEN%\Core\Src\stm32f1xx_it.c"  Core\Src\stm32f1xx_it.c  >nul
copy /y "%GOLDEN%\Core\Inc\main.h"          Core\Inc\main.h          >nul
copy /y "%GOLDEN%\Core\Inc\stm32f1xx_it.h"  Core\Inc\stm32f1xx_it.h  >nul
echo   ok Core\Src\main.c
echo   ok Core\Src\stm32f1xx_it.c
echo   ok Core\Inc\main.h
echo   ok Core\Inc\stm32f1xx_it.h

echo.
echo Step 3: Verifying additional application files exist...
for %%f in (Core\Inc\config.h Core\Inc\variables.h Core\Inc\loop_detector.h Core\Inc\classification.h Core\Inc\interval.h Core\Inc\protocol.h Core\Inc\air780_tcp.h Core\Inc\storage.h Core\Inc\w25q80.h Core\Inc\sd_spi.h Core\Src\loop_detector.c Core\Src\classification.c Core\Src\interval.c Core\Src\protocol.c Core\Src\air780_tcp.c Core\Src\storage.c Core\Src\w25q80.c Core\Src\sd_spi.c) do (
    if not exist "%%f" (
        echo   [MISSING] %%f
        pause
        exit /b 1
    )
)
echo   ok all application sources present

echo.
echo ──────────────────────────────────────────────
echo  Setup complete!
echo.
echo  Next steps:
echo    1) In CubeIDE: right-click project, Refresh (F5)
echo    2) Build (Ctrl+B)
echo    3) Connect ST-Link, then Run (Ctrl+F11)
echo ──────────────────────────────────────────────
pause
