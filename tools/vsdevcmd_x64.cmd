@echo off
setlocal

set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" (
  echo [ERROR] Nie znaleziono VsDevCmd: %VSDEV%
  exit /b 1
)

call "%VSDEV%" -arch=x64 -host_arch=x64
if errorlevel 1 (
  echo [ERROR] Nie udalo sie zaladowac srodowiska VS.
  exit /b 1
)

echo [OK] Srodowisko MSVC x64 zaladowane.
echo [INFO] Mozesz uruchomic: cl /?, cmake --build, msbuild, itp.
cmd /k
