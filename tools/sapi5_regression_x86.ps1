$tests = @(
  @{name='stmt'; text='To jest zdanie oznajmujące.'},
  @{name='question'; text='To jest pytanie?'},
  @{name='exclaim'; text='To jest okrzyk!'},
  @{name='numbers'; text='Liczby: 1 2 3 4 5 6 10 12 25 101 1000.'},
  @{name='punct'; text='Przecinek, średnik; dwukropek: kropka.'}
)
$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices()
$sel = $null
for($i=0; $i -lt $voices.Count; $i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel = $voices.Item($i); break } }
if($null -eq $sel){ throw 'BlackBox voice not found x86' }
$v.Voice = $sel
foreach($t in $tests){
  $out = "test_outputs\\sapi5_x86_$($t.name).wav"
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $out, 3, $false)
  $v.AudioOutputStream = $fs
  $null = $v.Speak($t.text, 0)
  $fs.Close()
}
Get-ChildItem test_outputs\sapi5_x86_*.wav | Select-Object Name,Length,LastWriteTime
