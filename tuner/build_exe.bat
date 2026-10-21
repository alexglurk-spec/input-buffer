@echo off
rem Builds "GD Input Buffer Tuner.exe" from buffer_tuner.py.
rem Put this file in the same folder as buffer_tuner.py and double-click it.

cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo Python was not found. Install Python 3 from python.org and tick "Add python.exe to PATH" in the installer, then run this again.
    pause
    exit /b 1
)

echo Installing PyInstaller...
python -m pip install --upgrade pyinstaller
if errorlevel 1 (
    echo Could not install PyInstaller. Check your internet connection and try again.
    pause
    exit /b 1
)

echo Building the program...
python -m PyInstaller --onefile --windowed --noconfirm --name "GD Input Buffer Tuner" buffer_tuner.py
if errorlevel 1 (
    echo The build failed. Scroll up to see the error.
    pause
    exit /b 1
)

echo.
echo Done. Your program is here: dist\GD Input Buffer Tuner.exe
echo You can copy that single file anywhere. It does not need Python to run.
pause
