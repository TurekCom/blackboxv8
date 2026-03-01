$token='HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5'
Set-ItemProperty $token -Name VoiceFlavor -Value 'c64'
Set-ItemProperty $token -Name IntonationMode -Value 'auto'
Set-ItemProperty $token -Name IntonationStrength -Value 110
Set-ItemProperty $token -Name SymbolLevel -Value 'most'
Set-ItemProperty $token -Name NumberMode -Value 'cardinal'

$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
if($null -eq $sel){ throw 'voice not found' }
$v.Voice = $sel

$tests = @(
  @{name='mateusz'; text='Mateusz'},
  @{name='sz_only'; text='sz szum szeroki szlak'},
  @{name='t_only'; text='t twardy takt tatra tytan'},
  @{name='phones_focus'; text='ś dź dż ć ł ę rz. Mateusz, szczęście, cztery, twardy takt.'}
)
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x64_focus_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text,0)
  $v.WaitUntilDone(12000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_focus_*.wav | Select-Object Name,Length,LastWriteTime
