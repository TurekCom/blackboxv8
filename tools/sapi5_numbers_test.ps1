$items=@('1','12','25','101','1000')
$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
if($null -eq $sel){ throw 'voice missing' }
$v.Voice = $sel
foreach($t in $items){
  $safe=$t.Replace('-','m')
  $out="test_outputs\\sapi5_x64_num_$safe.wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t,0)
  $v.WaitUntilDone(5000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_num_*.wav | Select-Object Name,Length
