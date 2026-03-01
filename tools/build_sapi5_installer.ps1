param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "== Build SAPI5 dual-arch installer =="
cmake -S sapi5 -B sapi5\build-x64 -G "Visual Studio 17 2022" -A x64 | Out-Host
cmake --build sapi5\build-x64 --config $Config | Out-Host

cmake -S sapi5 -B sapi5\build-x86 -G "Visual Studio 17 2022" -A Win32 | Out-Host
cmake --build sapi5\build-x86 --config $Config | Out-Host

iscc /Qp installer\blackbox_sapi5.iss | Out-Host

Write-Host "== Done =="
Get-ChildItem dist\installer\BlackBoxSapi5-*.exe | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
