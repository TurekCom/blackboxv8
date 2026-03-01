$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
$v.Voice = $sel
$cases=@(@{n='with_numbers';t='Liczby 1 2 3 4 5.'},@{n='without_numbers';t='Liczby.'})
foreach($c in $cases){
  $out="test_outputs\\sapi5_x64_cmp_$($c.n).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak([string]$c.t,0)
  $v.WaitUntilDone(5000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_cmp_*.wav | Select-Object Name,Length
