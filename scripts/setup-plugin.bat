@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set TEMPLATE_DIR=obs-plugintemplate-build
set PLUGIN_NAME=obs-overlay-time-show

where git >nul 2>&1
if errorlevel 1 (
  echo Git nao encontrado. Instale Git para clonar o template do OBS.
  exit /b 1
)

if not exist "%TEMPLATE_DIR%\.git" (
  echo Clonando obs-plugintemplate...
  git clone --depth 1 https://github.com/obsproject/obs-plugintemplate.git "%TEMPLATE_DIR%"
  if errorlevel 1 exit /b 1
)

echo Copiando codigo do plugin...
xcopy /E /I /Y "native-plugin\src" "%TEMPLATE_DIR%\src" >nul
xcopy /E /I /Y "native-plugin\data" "%TEMPLATE_DIR%\data" >nul
copy /Y "native-plugin\buildspec.json" "%TEMPLATE_DIR%\buildspec.json" >nul
copy /Y "native-plugin\CMakeLists.txt" "%TEMPLATE_DIR%\CMakeLists.txt" >nul

echo.
echo Arquivos copiados para %TEMPLATE_DIR%
echo.
echo Proximos passos:
echo   1. Instale Visual Studio 2022 com "Desktop development with C++"
echo   2. Instale CMake 3.28+
echo   3. Configure o caminho do codigo-fonte do OBS no CMakeUserPresets.json
echo   4. cd %TEMPLATE_DIR%
echo   5. cmake --preset windows-x64
echo   6. cmake --build --preset windows-x64
echo.
echo Depois rode Build-Install-Plugin.bat na raiz do projeto.
pause
