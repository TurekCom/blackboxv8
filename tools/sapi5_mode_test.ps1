$token='HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5'
$text='Test intonacji bez znaku końcowego'
function SpeakTo([string]$name){
  $v = New-Object -ComObject SAPI.SpVoice
  $voices = $v.GetVoices(); $sel=$null
  for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
  if($null -eq $sel){ throw 'voice not found' }
  $v.Voice = $sel
  $out="test_outputs\\$name"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($text,0)
  $fs.Close()
}
Set-ItemProperty $token -Name IntonationMode -Value 'flat'
Set-ItemProperty $token -Name IntonationStrength -Value 100
SpeakTo 'sapi5_x64_mode_flat.wav'
Set-ItemProperty $token -Name IntonationMode -Value 'question'
SpeakTo 'sapi5_x64_mode_question.wav'
Set-ItemProperty $token -Name IntonationMode -Value 'auto'
Get-ChildItem test_outputs\sapi5_x64_mode_*.wav | Select-Object Name,Length,LastWriteTime
