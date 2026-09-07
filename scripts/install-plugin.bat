@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

set "PLUGIN_DLL=obs-overlay-time-show.dll"
set "PLUGIN_DATA=obs-overlay-time-show"

set "DLL_SRC="
if exist "build-output\%PLUGIN_DLL%" set "DLL_SRC=%CD%\build-output\%PLUGIN_DLL%"
if not defined DLL_SRC if exist "dist\%PLUGIN_DLL%" set "DLL_SRC=%CD%\dist\%PLUGIN_DLL%"

if not defined DLL_SRC (
  echo Plugin compilado nao encontrado.
  echo Use Build-Install-Plugin.bat na raiz do projeto.
  pause
  exit /b 1
)

tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | find /I "obs64.exe" >nul
if %ERRORLEVEL%==0 (
  echo O OBS esta aberto. Feche o OBS antes de instalar o plugin.
  pause
  exit /b 1
)

set "OBS_ROOT="
if defined OBS_STUDIO_PATH set "OBS_ROOT=%OBS_STUDIO_PATH%"
if not defined OBS_ROOT if exist "D:\SteamLibrary\steamapps\common\OBS Studio\bin\64bit\obs64.exe" (
  set "OBS_ROOT=D:\SteamLibrary\steamapps\common\OBS Studio"
)
if not defined OBS_ROOT if exist "%ProgramFiles%\obs-studio\bin\64bit\obs64.exe" set "OBS_ROOT=%ProgramFiles%\obs-studio"
if not defined OBS_ROOT if exist "%ProgramFiles(x86)%\obs-studio\bin\64bit\obs64.exe" set "OBS_ROOT=%ProgramFiles(x86)%\obs-studio"

if not defined OBS_ROOT (
  echo Nao encontrei o OBS automaticamente.
  echo.
  echo Defina OBS_STUDIO_PATH, por exemplo:
  echo   D:\SteamLibrary\steamapps\common\OBS Studio
  echo.
  pause
  exit /b 1
)

echo Instalando em:
echo   %OBS_ROOT%
echo.

rem Atualiza so arquivos do plugin. Nao toca AppData (posicao) nem atalhos/cenas.
if exist "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%" del /F /Q "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%"
if exist "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%.pdb" del /F /Q "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%.pdb"
if exist "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%" rd /S /Q "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%"

mkdir "%OBS_ROOT%\obs-plugins\64bit" 2>nul
mkdir "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%\locale" 2>nul

copy /Y "%DLL_SRC%" "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%"
if errorlevel 1 (
  echo Falha ao copiar a DLL. Feche o OBS e tente novamente.
  pause
  exit /b 1
)

if exist "native-plugin\data\locale\*" (
  xcopy /E /I /Y "native-plugin\data\locale\*" "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%\locale\" >nul
) else if exist "dist\data\locale\*" (
  xcopy /E /I /Y "dist\data\locale\*" "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%\locale\" >nul
)

echo.
echo Plugin instalado com sucesso.
echo Posicao e atalhos do OBS foram preservados.
echo Reinicie o OBS e inicie uma gravacao para ver o timer.
echo Atalhos: Configuracoes ^> Atalhos ^> busque "Timer OBS"
echo.
pause
endlocal
