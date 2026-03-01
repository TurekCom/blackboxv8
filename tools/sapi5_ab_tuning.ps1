$token='HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5'
Set-ItemProperty $token -Name IntonationMode -Value 'auto'
Set-ItemProperty $token -Name IntonationStrength -Value 100
Set-ItemProperty $token -Name SpeakPunctuation -Value '0'
Set-ItemProperty $token -Name VoiceFlavor -Value 'c64'

$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
if($null -eq $sel){ throw 'voice not found' }
$v.Voice = $sel

$tests = @(
  @{name='cluster'; text='Przestrzeń, wstrząs, chrząszcz brzmi w trzcinie.'},
  @{name='question'; text='Czy naprawdę to działa poprawnie?'},
  @{name='exclaim'; text='Świetnie, to brzmi znacznie lepiej!'},
  @{name='numbers'; text='Liczby: 1 2 3 4 5 6 10 12 25 101 1000.'}
)
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x64_c64_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text,0)
  $v.WaitUntilDone(10000) | Out-Null
  $fs.Close()
}

Set-ItemProperty $token -Name VoiceFlavor -Value 'clear'
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x64_clear_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text,0)
  $v.WaitUntilDone(10000) | Out-Null
  $fs.Close()
}
Set-ItemProperty $token -Name VoiceFlavor -Value 'c64'
Get-ChildItem test_outputs\sapi5_x64_c64_*.wav, test_outputs\sapi5_x64_clear_*.wav | Sort-Object Name | Select-Object Name,Length
