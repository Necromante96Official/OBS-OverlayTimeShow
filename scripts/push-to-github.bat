@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

where git >nul 2>&1
if errorlevel 1 (
  echo Git nao encontrado. Instale: https://git-scm.com/download/win
  pause
  exit /b 1
)

if not exist ".git" (
  echo Inicializando repositorio git...
  git init -b main
  if errorlevel 1 (
    git init
    git branch -M main
  )
)

git remote remove origin 2>nul
git remote add origin https://github.com/Necromante96Official/OBS-OverlayTimeShow.git

echo.
echo Arquivos que serao enviados (config.json e .venv NAO entram):
git add -A
git status

echo.
set /p CONFIRM=Confirmar commit e push para o GitHub? [S/N]:
if /I not "%CONFIRM%"=="S" (
  echo Cancelado.
  pause
  exit /b 0
)

git diff --cached --quiet
if errorlevel 1 (
  git commit -m "Initial commit: OBS Overlay Time Show plugin and overlay"
) else (
  echo Nada novo para commitar.
)

echo.
echo Enviando para GitHub...
git push -u origin main
if errorlevel 1 (
  echo.
  echo Push falhou. Se pediu autenticacao:
  echo   1. Instale GitHub CLI: winget install GitHub.cli
  echo   2. Execute: gh auth login
  echo   3. Rode este script de novo
  echo.
  echo Ou use um Personal Access Token como senha no prompt do Git.
  pause
  exit /b 1
)

echo.
echo Pronto: https://github.com/Necromante96Official/OBS-OverlayTimeShow
pause
