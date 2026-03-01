param()

$ErrorActionPreference = "Stop"

function Test-Tool($name) {
    try {
        $cmd = Get-Command $name -ErrorAction Stop
        Write-Host "[OK] $name -> $($cmd.Source)"
        return $true
    } catch {
        Write-Host "[MISS] $name"
        return $false
    }
}

Write-Host "== SAPI5 / C++ environment check =="
$hasCmake = Test-Tool "cmake"
$hasNinja = Test-Tool "ninja"
$hasIscc = Test-Tool "iscc"
if ($hasCmake) { cmd /c "cmake --version" 2>$null | Select-Object -First 2 | ForEach-Object { Write-Host "      $_" } }
if ($hasNinja) { cmd /c "ninja --version" 2>$null | Select-Object -First 1 | ForEach-Object { Write-Host "      $_" } }
if ($hasIscc) { cmd /c "iscc /?" 2>$null | Select-Object -First 1 | ForEach-Object { Write-Host "      $_" } }

$vsDev = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if (Test-Path $vsDev) {
    Write-Host "[OK] VsDevCmd -> $vsDev"
    $cmd = "`"$vsDev`" -arch=x64 -host_arch=x64 >nul && cl"
    cmd /c $cmd | Select-Object -First 3 | ForEach-Object { Write-Host "      $_" }
} else {
    Write-Host "[MISS] VsDevCmd (VS 2022 Community)"
}

$sdkRoot = "C:\Program Files (x86)\Windows Kits\10\Include"
if (Test-Path $sdkRoot) {
    $latest = Get-ChildItem $sdkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($null -ne $latest) {
        $sapiHeader = Join-Path $latest.FullName "um\sapi.h"
        if (Test-Path $sapiHeader) {
            Write-Host "[OK] sapi.h -> $sapiHeader"
        } else {
            Write-Host "[MISS] sapi.h in latest Windows SDK include dir"
        }
    }
}

Write-Host "== done =="
