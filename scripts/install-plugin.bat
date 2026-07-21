@echo off
setlocal
cd /d "%~dp0.."

set "DLL_SRC="
if exist "build-output\obs-overlay-time-show.dll" set "DLL_SRC=%CD%\build-output\obs-overlay-time-show.dll"
if not defined DLL_SRC if exist "dist\obs-overlay-time-show.dll" set "DLL_SRC=%CD%\dist\obs-overlay-time-show.dll"

if not defined DLL_SRC (
  echo Plugin compilado nao encontrado.
  echo Compile primeiro com scripts\rebuild-plugin.bat
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
if not defined OBS_ROOT if exist "%ProgramFiles%\obs-studio\bin\64bit\obs64.exe" set "OBS_ROOT=%ProgramFiles%\obs-studio"
if not defined OBS_ROOT if exist "%ProgramFiles(x86)%\obs-studio\bin\64bit\obs64.exe" set "OBS_ROOT=%ProgramFiles(x86)%\obs-studio"

if not defined OBS_ROOT (
  echo Nao encontrei o OBS automaticamente.
  echo.
  echo Defina a variavel de ambiente OBS_STUDIO_PATH com a pasta do OBS,
  echo por exemplo: C:\Program Files\obs-studio
  echo.
  echo Se o OBS veio pela Steam, use a pasta onde esta o obs64.exe
  echo ^(a pasta pai de bin\64bit^).
  echo.
  pause
  exit /b 1
)

mkdir "%OBS_ROOT%\obs-plugins\64bit" 2>nul
mkdir "%OBS_ROOT%\data\obs-plugins\obs-overlay-time-show\locale" 2>nul

copy /Y "%DLL_SRC%" "%OBS_ROOT%\obs-plugins\64bit\obs-overlay-time-show.dll"
if errorlevel 1 (
  echo Falha ao copiar a DLL. Feche o OBS e tente novamente.
  pause
  exit /b 1
)

if exist "native-plugin\data\locale\*" (
  xcopy /E /I /Y "native-plugin\data\locale\*" "%OBS_ROOT%\data\obs-plugins\obs-overlay-time-show\locale\" >nul
) else if exist "dist\data\locale\*" (
  xcopy /E /I /Y "dist\data\locale\*" "%OBS_ROOT%\data\obs-plugins\obs-overlay-time-show\locale\" >nul
)

echo.
echo Plugin instalado com sucesso.
echo Reinicie o OBS e inicie uma gravacao para ver o timer.
echo Atalhos: Configuracoes ^> Atalhos ^> busque "Timer OBS"
echo.
pause
