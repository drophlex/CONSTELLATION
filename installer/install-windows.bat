@echo off
:: CONSTELLATION installer for Windows — Polaris Sound Lab
net session >nul 2>&1
if %errorLevel% neq 0 (
  powershell -Command "Start-Process '%~f0' -Verb RunAs"
  exit /b
)
cd /d "%~dp0"
echo Installing CONSTELLATION...
if exist "%CommonProgramFiles%\VST3\Constellation.vst3" rmdir /s /q "%CommonProgramFiles%\VST3\Constellation.vst3"
xcopy /e /i /y "Constellation.vst3" "%CommonProgramFiles%\VST3\Constellation.vst3" >nul
echo.
echo Done! / Listo!
echo Ableton: Options - Preferences - Plug-Ins - turn on "Use VST3 Plug-in System Folders" and Rescan.
echo Ableton: Opciones - Preferencias - Plug-Ins - activa "Usar carpetas de sistema VST3" y Re-escanear.
pause
