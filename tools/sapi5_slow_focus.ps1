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
$v.Rate = -8   # at least ~2x slower than default for intelligibility checks

$tests = @(
  @{name='mateusz_slow'; text='Mateusz. Mateusz. Mateusz.'},
  @{name='usz_slow'; text='usz usz usz. Mate-usz. Teusz.'},
  @{name='sz_t_slow'; text='sz sz sz. t t t. szczęście, cztery, twardy takt.'},
  @{name='phones_slow'; text='ś dź dż ć ł ę rz. ś dź dż ć ł ę rz.'}
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
Get-ChildItem test_outputs\sapi5_x64_*_slow.wav | Select-Object Name,Length,LastWriteTime
