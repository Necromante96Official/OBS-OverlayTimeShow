@echo off
setlocal
cd /d "%~dp0"

set "OBS_ROOT=D:\SteamLibrary\steamapps\common\OBS Studio"
set "DLL_SRC=%~dp0dist\obs-overlay-time-show.dll"

if not exist "%DLL_SRC%" (
  echo Plugin compilado nao encontrado em dist\. Execute o build primeiro.
  pause
  exit /b 1
)

tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | find /I "obs64.exe" >nul
if %ERRORLEVEL%==0 (
  echo.
  echo O OBS esta aberto. Feche o OBS antes de instalar o plugin.
  echo.
  pause
  exit /b 1
)

if not exist "%OBS_ROOT%\bin\64bit\obs64.exe" (
  echo OBS nao encontrado em:
  echo   %OBS_ROOT%
  echo.
  echo Edite OBS_ROOT neste arquivo .bat com o caminho do seu OBS.
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

xcopy /E /I /Y "%~dp0dist\data\locale\*" "%OBS_ROOT%\data\obs-plugins\obs-overlay-time-show\locale\" >nul

echo.
echo Plugin instalado com sucesso.
echo Abra o OBS e procure em Configuracoes ^> Atalhos:
echo   "Timer OBS: mover para cima/baixo/esquerda/direita"
echo.
pause
