@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "PATH=%ProgramFiles%\CMake\bin;%PATH%"

echo Copiando fontes...
copy /Y "native-plugin\src\recording-timer-overlay.cpp" "obs-plugintemplate-build\src\" >nul
copy /Y "native-plugin\src\recording-timer-overlay.hpp" "obs-plugintemplate-build\src\" >nul
copy /Y "native-plugin\src\plugin-main.cpp" "obs-plugintemplate-build\src\" >nul
xcopy /E /I /Y "native-plugin\data\locale\*" "obs-plugintemplate-build\data\locale\" >nul

echo Compilando...
cd obs-plugintemplate-build
cmake --build --preset windows-x64 --parallel
if errorlevel 1 (
  echo Build falhou.
  pause
  exit /b 1
)

cd /d "%~dp0.."
mkdir build-output 2>nul
mkdir build-output\data\locale 2>nul
mkdir dist 2>nul
mkdir dist\data\locale 2>nul

copy /Y "obs-plugintemplate-build\build_x64\RelWithDebInfo\obs-overlay-time-show.dll" "build-output\" >nul
xcopy /E /I /Y "native-plugin\data\locale\*" "build-output\data\locale\" >nul
copy /Y "build-output\obs-overlay-time-show.dll" "dist\" >nul
xcopy /E /I /Y "build-output\data\locale\*" "dist\data\locale\" >nul

echo.
echo Build OK. Instalando...
call "%~dp0install-plugin.bat"
