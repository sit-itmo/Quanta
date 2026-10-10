@echo off
setlocal
rem Usage: tools\build_windows.cmd [ON|OFF] [x64|Win32]
set "GUI=%~1"
if "%GUI%"=="" set "GUI=OFF"
set "ARCH=%~2"
if "%ARCH%"=="" set "ARCH=x64"
set "ROOT=%~dp0.."
set "BUILD=%ROOT%\out\%ARCH%-gui-%GUI%"
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A "%ARCH%" -DINV_WITH_GUI=%GUI% -DINV_STRICT_STATIC=ON
if errorlevel 1 exit /b 1
cmake --build "%BUILD%" --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir "%BUILD%" -C Release --output-on-failure
if errorlevel 1 exit /b 1
echo Executable: "%BUILD%\Release\inv.exe"
endlocal
