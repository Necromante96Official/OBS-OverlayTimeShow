@echo off
rem Carrega CMake, Git e MSVC no PATH da sessao atual.
rem Uso: call "%~dp0ensure-build-env.bat"

set "VSCMD_SKIP_SENDTELEMETRY=1"
set "PATH=%ProgramFiles%\CMake\bin;%ProgramFiles%\Git\cmd;%ProgramFiles%\Git\bin;%PATH%"

rem Python 3.12 (usuario)
if exist "%LocalAppData%\Programs\Python\Python312" (
  set "PATH=%LocalAppData%\Programs\Python\Python312;%LocalAppData%\Programs\Python\Python312\Scripts;%PATH%"
)
if exist "%LocalAppData%\Programs\Python\Launcher" (
  set "PATH=%LocalAppData%\Programs\Python\Launcher;%PATH%"
)

rem Localiza Visual Studio / Build Tools via vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSINSTALL="
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if not defined VSINSTALL set "VSINSTALL=%%I"
  )
)

if not defined VSINSTALL if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" set "VSINSTALL=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
if not defined VSINSTALL if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools" set "VSINSTALL=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
if not defined VSINSTALL if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" set "VSINSTALL=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if not defined VSINSTALL if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional" set "VSINSTALL=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
if not defined VSINSTALL if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community" set "VSINSTALL=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"

if defined VSINSTALL (
  if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
  )
  if exist "%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" (
    set "PATH=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
  )
  if exist "%VSINSTALL%\MSBuild\Current\Bin\amd64" (
    set "PATH=%VSINSTALL%\MSBuild\Current\Bin\amd64;%PATH%"
  ) else if exist "%VSINSTALL%\MSBuild\Current\Bin" (
    set "PATH=%VSINSTALL%\MSBuild\Current\Bin;%PATH%"
  )
)

rem Fallback: adiciona o cl.exe mais recente se VsDevCmd nao carregou
where cl >nul 2>&1
if errorlevel 1 if defined VSINSTALL if exist "%VSINSTALL%\VC\Tools\MSVC" (
  for /f "delims=" %%V in ('dir /b /ad /o-n "%VSINSTALL%\VC\Tools\MSVC"') do (
    if exist "%VSINSTALL%\VC\Tools\MSVC\%%V\bin\Hostx64\x64\cl.exe" (
      set "PATH=%VSINSTALL%\VC\Tools\MSVC\%%V\bin\Hostx64\x64;%PATH%"
      goto :after_cl
    )
  )
)
:after_cl

where cmake >nul 2>&1
if errorlevel 1 (
  echo [ensure-build-env] CMake nao encontrado no PATH.
  exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
  echo [ensure-build-env] Compilador C++ ^(cl^) nao encontrado.
  echo Rode Setup-Build-Env.bat e reinicie o terminal.
  exit /b 1
)

exit /b 0
