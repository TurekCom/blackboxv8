$items=@('abc','1','12')
$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ $d=$voices.Item($i).GetDescription(); if($d -like '*BlackBox*'){ $sel=$voices.Item($i); Write-Host "SEL=$d"; break } }
if($null -eq $sel){ throw 'voice missing' }
$v.Voice = $sel
foreach($t in $items){
  $safe=$t.Replace('-','m')
  $out="test_outputs\\sapi5_x64_dbg_$safe.wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $ret=$v.Speak($t,0)
  Write-Host "Speak '$t' ret=$ret"
  $v.WaitUntilDone(5000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_dbg_*.wav | Select-Object Name,Length
