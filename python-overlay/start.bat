@echo off
setlocal
cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
  echo Python nao encontrado. Instale Python 3.10+ em https://www.python.org/
  echo Marque a opcao "Add python.exe to PATH" na instalacao.
  pause
  exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
  echo Criando ambiente virtual...
  python -m venv .venv
  call .venv\Scripts\activate.bat
  python -m pip install --upgrade pip
  pip install -r requirements.txt
) else (
  call .venv\Scripts\activate.bat
  pip install -q -r requirements.txt
)

python main.py
