$ErrorActionPreference = "Stop"

Write-Host "== SAPI5 voice diagnostics =="

$token64 = "HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5"
$token32 = "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5"

Write-Host ("Token 64-bit exists: " + (Test-Path $token64))
if (Test-Path $token64) {
    $v = Get-ItemProperty $token64
    Write-Host ("  CLSID: " + $v.CLSID)
}

Write-Host ("Token 32-bit exists: " + (Test-Path $token32))
if (Test-Path $token32) {
    $v = Get-ItemProperty $token32
    Write-Host ("  CLSID: " + $v.CLSID)
}

Write-Host "-- Voices via 64-bit SpVoice --"
$v64 = New-Object -ComObject SAPI.SpVoice
$voices64 = $v64.GetVoices()
for ($i = 0; $i -lt $voices64.Count; $i++) {
    Write-Host ("  " + $voices64.Item($i).GetDescription())
}

Write-Host "-- Voices via 32-bit SpVoice --"
$scriptPath = Join-Path $env:TEMP "bb_sapi5_wow_check.ps1"
$wowScript = @"
`$v = New-Object -ComObject SAPI.SpVoice
`$voices = `$v.GetVoices()
for(`$i=0; `$i -lt `$voices.Count; `$i++){
  Write-Host ("  " + `$voices.Item(`$i).GetDescription())
}
"@
$wowScript | Set-Content -Encoding ASCII $scriptPath

& "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $scriptPath
Remove-Item $scriptPath -Force

Write-Host "== done =="
