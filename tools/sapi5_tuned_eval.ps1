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
  @{name='clusters2'; text='Szczęście, cztery kropki, twardy takt, tekst krótki.'},
  @{name='polishphones'; text='ś dź dż ć ł ę rz, chrząszcz brzmi w trzcinie.'},
  @{name='medium_pitch'; text='To jest test domyślnej wysokości i wyrazistości syntezatora.'}
)
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x64_tuned_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text,0)
  $v.WaitUntilDone(12000) | Out-Null
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x64_tuned_*.wav | Select-Object Name,Length,LastWriteTime
