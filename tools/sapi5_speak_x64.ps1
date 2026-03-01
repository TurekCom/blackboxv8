$out='test_outputs\\sapi5_blackbox_x64.wav'
$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices()
$sel = $null
for($i=0; $i -lt $voices.Count; $i++){
  $t = $voices.Item($i)
  if($t.GetDescription() -like '*BlackBox*'){ $sel = $t; break }
}
if($null -eq $sel){ throw 'BlackBox voice not found (x64)' }
$v.Voice = $sel
$fs = New-Object -ComObject SAPI.SpFileStream
$fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
$v.AudioOutputStream = $fs
$null = $v.Speak('To jest test syntezy BlackBox V8 przez SAPI5.', 0)
$fs.Close()
Get-Item $out | Select-Object FullName,Length,LastWriteTime
