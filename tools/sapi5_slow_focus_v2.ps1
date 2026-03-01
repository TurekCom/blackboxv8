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
$v.Rate = -8

$tests = @(
  @{name='mateusz_slow_v2'; text='Mateusz. Mateusz.'},
  @{name='usz_slow_v2'; text='usz usz usz.'},
  @{name='sz_slow_v2'; text='sz sz sz. szeroki szlak. cisza i szum.'},
  @{name='rz_z_slow_v2'; text='żaba, rzeka, burza, marzę, wrażliwy.'}
)
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x64_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text,0)
  $v.WaitUntilDone(30000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_*_v2.wav | Select-Object Name,Length,LastWriteTime
