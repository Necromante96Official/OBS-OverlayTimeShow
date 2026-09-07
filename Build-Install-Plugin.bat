@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem ============================================================
rem  Compila o plugin e instala no OBS (Steam).
rem  Atualiza so a DLL e os textos do plugin.
rem  NAO apaga posicao salva, atalhos nem cenas do OBS.
rem ============================================================

set "OBS_ROOT=D:\SteamLibrary\steamapps\common\OBS Studio"
set "PLUGIN_DLL=obs-overlay-time-show.dll"
set "PLUGIN_DATA=obs-overlay-time-show"

echo.
echo ========================================
echo  OBS Overlay Time Show - Build + Install
echo ========================================
echo.
echo OBS destino:
echo   %OBS_ROOT%
echo.

if not exist "%OBS_ROOT%\bin\64bit\obs64.exe" (
  echo ERRO: OBS nao encontrado em:
  echo   %OBS_ROOT%
  echo.
  echo Confirme o caminho da instalacao Steam.
  pause
  exit /b 1
)

tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | find /I "obs64.exe" >nul
if %ERRORLEVEL%==0 (
  echo O OBS esta aberto. Feche o OBS e rode este script de novo.
  pause
  exit /b 1
)

echo Carregando ferramentas de build no PATH...
call "%~dp0scripts\ensure-build-env.bat"
if errorlevel 1 (
  echo.
  echo Ambiente incompleto. Rode uma vez: Setup-Build-Env.bat
  echo.
  pause
  exit /b 1
)

rem Prefere o CMake do Kitware ^(evita mistura 4.3 do VS com 4.4^)
set "CMAKE_EXE="
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE_EXE (
  where cmake >nul 2>&1 && for /f "delims=" %%I in ('where cmake') do (
    if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
  )
)

if not defined CMAKE_EXE (
  echo ERRO: CMake nao encontrado.
  echo Rode Setup-Build-Env.bat e tente de novo.
  pause
  exit /b 1
)

if not exist "obs-plugintemplate-build\CMakeLists.txt" (
  echo Pasta de build do template nao encontrada.
  echo Rodando setup do plugin...
  call "%~dp0scripts\setup-plugin.bat"
  if errorlevel 1 (
    echo Setup falhou.
    pause
    exit /b 1
  )
)

echo [1/4] Copiando fontes atualizadas...
xcopy /E /I /Y "native-plugin\src\*" "obs-plugintemplate-build\src\" >nul
copy /Y "native-plugin\buildspec.json" "obs-plugintemplate-build\buildspec.json" >nul
copy /Y "native-plugin\CMakeLists.txt" "obs-plugintemplate-build\CMakeLists.txt" >nul
if exist "native-plugin\data\locale" (
  xcopy /E /I /Y "native-plugin\data\locale\*" "obs-plugintemplate-build\data\locale\" >nul
)

rem Se o cache aponta para outra versao do CMake, limpa e reconfigura
set "NEED_RECONFIGURE=0"
if not exist "obs-plugintemplate-build\build_x64\CMakeCache.txt" set "NEED_RECONFIGURE=1"
if exist "obs-plugintemplate-build\build_x64\CMakeCache.txt" (
  findstr /C:"cmake-4.3" "obs-plugintemplate-build\build_x64\CMakeCache.txt" >nul 2>&1
  if not errorlevel 1 set "NEED_RECONFIGURE=1"
  findstr /C:"CMAKE_HOME_DIRECTORY:INTERNAL=" "obs-plugintemplate-build\build_x64\CMakeCache.txt" >nul 2>&1
)

if "!NEED_RECONFIGURE!"=="1" if exist "obs-plugintemplate-build\build_x64" (
  echo Limpando cache CMake antigo ^(incompativel^)...
  rd /S /Q "obs-plugintemplate-build\build_x64"
)

echo [2/4] Compilando com:
echo   !CMAKE_EXE!
pushd "obs-plugintemplate-build"

if not exist "build_x64\CMakeCache.txt" (
  echo Configurando CMake...
  "!CMAKE_EXE!" --preset windows-x64
  if errorlevel 1 (
    popd
    echo.
    echo Configuracao CMake falhou.
    pause
    exit /b 1
  )
)

"!CMAKE_EXE!" --build --preset windows-x64 --parallel
set "BUILD_ERR=!ERRORLEVEL!"
popd
if not "!BUILD_ERR!"=="0" (
  echo.
  echo Build falhou ^(codigo !BUILD_ERR!^).
  echo Nada foi instalado no OBS.
  pause
  exit /b 1
)

set "DLL_BUILT="
if exist "obs-plugintemplate-build\build_x64\RelWithDebInfo\%PLUGIN_DLL%" (
  set "DLL_BUILT=%CD%\obs-plugintemplate-build\build_x64\RelWithDebInfo\%PLUGIN_DLL%"
)
if not defined DLL_BUILT if exist "obs-plugintemplate-build\build_x64\Release\%PLUGIN_DLL%" (
  set "DLL_BUILT=%CD%\obs-plugintemplate-build\build_x64\Release\%PLUGIN_DLL%"
)

if not defined DLL_BUILT (
  echo ERRO: Build terminou, mas a DLL nao foi gerada.
  pause
  exit /b 1
)

echo [3/4] Preparando saida local...
mkdir "build-output" 2>nul
mkdir "build-output\data\locale" 2>nul
mkdir "dist" 2>nul
mkdir "dist\data\locale" 2>nul
copy /Y "!DLL_BUILT!" "build-output\%PLUGIN_DLL%" >nul
copy /Y "!DLL_BUILT!" "dist\%PLUGIN_DLL%" >nul
if exist "native-plugin\data\locale" (
  xcopy /E /I /Y "native-plugin\data\locale\*" "build-output\data\locale\" >nul
  xcopy /E /I /Y "native-plugin\data\locale\*" "dist\data\locale\" >nul
)

echo [4/4] Atualizando plugin no OBS...
rem Remove so arquivos do plugin ^(DLL + data^). Nao toca AppData / cenas / atalhos.
if exist "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%" (
  del /F /Q "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%"
)
if exist "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%.pdb" (
  del /F /Q "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%.pdb"
)
if exist "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%" (
  rd /S /Q "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%"
)

mkdir "%OBS_ROOT%\obs-plugins\64bit" 2>nul
mkdir "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%\locale" 2>nul

copy /Y "!DLL_BUILT!" "%OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%"
if errorlevel 1 (
  echo Falha ao copiar a DLL. Feche o OBS e tente novamente.
  pause
  exit /b 1
)

if exist "native-plugin\data\locale" (
  xcopy /E /I /Y "native-plugin\data\locale\*" "%OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%\locale\" >nul
)

echo.
echo ========================================
echo  Pronto.
echo ========================================
echo  DLL:  %OBS_ROOT%\obs-plugins\64bit\%PLUGIN_DLL%
echo  Data: %OBS_ROOT%\data\obs-plugins\%PLUGIN_DATA%
echo.
echo  Posicao do timer e atalhos do OBS foram preservados
echo  ^(ficam no AppData / perfil do OBS^).
echo.
echo  Abra o OBS e inicie uma gravacao para testar.
echo ========================================
echo.
pause
endlocal
