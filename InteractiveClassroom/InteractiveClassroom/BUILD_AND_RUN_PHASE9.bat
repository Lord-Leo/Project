@echo off
setlocal
cd /d "%~dp0"

echo [1/4] Checking tools...
where cmake >nul 2>nul || (echo ERROR: CMake was not found. Open this project from Visual Studio 2022 Developer Command Prompt. & pause & exit /b 1)
where git >nul 2>nul || (echo ERROR: Git was not found in PATH. Restart Windows/terminal after installing Git. & pause & exit /b 1)

echo [2/4] Removing any incomplete CMake build...
if exist build rmdir /s /q build

echo [3/4] Configuring and building Phase 9...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || (echo CONFIGURE FAILED. & pause & exit /b 1)
cmake --build build --config Debug || (echo BUILD FAILED. & pause & exit /b 1)

echo [4/4] Starting Interactive Classroom Phase 9...
if not exist "build\Debug\InteractiveClassroom.exe" (echo ERROR: Executable was not created. & pause & exit /b 1)
"build\Debug\InteractiveClassroom.exe"
endlocal
