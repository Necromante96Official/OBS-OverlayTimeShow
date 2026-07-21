@echo off
setlocal
cd /d "%~dp0.."

echo Removendo pastas antigas (se existirem)...
if exist "overlay" rd /s /q "overlay"
if exist "obs-plugin" rd /s /q "obs-plugin"
if exist "obs-dock" rd /s /q "obs-dock"

if exist "_reorg-log.txt" del /q "_reorg-log.txt"
if exist "_reorg.py" del /q "_reorg.py"

echo.
echo Raiz do projeto agora:
dir /b /a:d
echo.
echo Pronto. Pastas novas: python-overlay, native-plugin, browser-dock, scripts, docs
pause
