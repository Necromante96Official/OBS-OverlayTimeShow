@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

rem ============================================================
rem  Instala / repara ferramentas de build e coloca no PATH.
rem  - CMake
rem  - Git ^(se faltar^)
rem  - Visual Studio 2022 Build Tools ^(C++^)
rem ============================================================

echo.
echo ========================================
echo  Setup do ambiente de build
echo ========================================
echo.

where winget >nul 2>&1
if errorlevel 1 (
  echo ERRO: winget nao encontrado. Atualize o App Installer da Microsoft Store.
  pause
  exit /b 1
)

echo [1/3] CMake...
where cmake >nul 2>&1
if errorlevel 1 (
  if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    echo CMake instalado, faltava no PATH. Vou adicionar.
  ) else (
    winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements --disable-interactivity
    if errorlevel 1 (
      echo Falha ao instalar CMake.
      pause
      exit /b 1
    )
  )
) else (
  echo CMake OK.
)

echo [2/3] Git...
where git >nul 2>&1
if errorlevel 1 (
  if exist "%ProgramFiles%\Git\cmd\git.exe" (
    echo Git instalado, faltava no PATH. Vou adicionar.
  ) else (
    winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements --disable-interactivity
    if errorlevel 1 (
      echo Falha ao instalar Git.
      pause
      exit /b 1
    )
  )
) else (
  echo Git OK.
)

echo [3/3] Visual Studio 2022 Build Tools ^(C++^)...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "HAS_MSVC=0"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "HAS_MSVC=1"
    echo MSVC encontrado em: %%I
  )
)

if "!HAS_MSVC!"=="0" (
  echo Instalando Build Tools. Pode demorar varios minutos...
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --disable-interactivity --override "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
  if errorlevel 1 (
    echo Falha ao instalar Visual Studio Build Tools.
    pause
    exit /b 1
  )
) else (
  echo Build Tools / MSVC OK.
)

echo.
echo Atualizando PATH do usuario...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update-user-path.ps1"
if errorlevel 1 (
  echo Aviso: nao foi possivel atualizar o PATH automaticamente.
)

echo.
echo ========================================
echo  Ambiente pronto.
echo ========================================
echo  Feche e reabra o terminal ^(ou o Cursor^) para o PATH valer em tudo.
echo  Depois rode: Build-Install-Plugin.bat
echo ========================================
echo.
pause
endlocal
