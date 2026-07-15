@echo off
setlocal

REM %1 = shader filename
set "SHADER=%~1"
set "OUT_DIR=cached"

if "%SHADER%"=="" (
    echo No shader file provided.
    pause
    exit /b 1
)

if not exist "%OUT_DIR%" (
    mkdir "%OUT_DIR%"
)

echo Compiling %SHADER%...

"%VULKAN_SDK%\Bin\glslc.exe" "%SHADER%" -o "%OUT_DIR%\%SHADER%.spv"
if errorlevel 1 (
    echo %SHADER% compilation failed!
    echo(
    exit /b 1
)

echo %SHADER% compiled to "%OUT_DIR%\%SHADER%.spv"
echo(