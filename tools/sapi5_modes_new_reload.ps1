function Speak-One($outfile,$text){
  $v = New-Object -ComObject SAPI.SpVoice
  $voices = $v.GetVoices(); $sel=$null
  for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
  if($null -eq $sel){ throw 'voice not found' }
  $v.Voice = $sel
  $fs = New-Object -ComObject SAPI.SpFileStream
  $fs.Open((Resolve-Path .).Path + '\\' + $outfile,3,$false)
  $v.AudioOutputStream = $fs
  $null=$v.Speak($text,0)
  $v.WaitUntilDone(10000)|Out-Null
  $fs.Close()
}

$token='HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5'
Set-ItemProperty $token -Name NumberMode -Value 'cardinal'
Set-ItemProperty $token -Name SymbolLevel -Value 'most'
Speak-One 'test_outputs\\sapi5_x64_cardinal_reload.wav' 'Liczby 123 i 45.'

Set-ItemProperty $token -Name NumberMode -Value 'digits'
Speak-One 'test_outputs\\sapi5_x64_digits_reload.wav' 'Liczby 123 i 45.'

Set-ItemProperty $token -Name SymbolLevel -Value 'none'
Speak-One 'test_outputs\\sapi5_x64_symbols_none_reload.wav' 'Ala, ma; kota: tak?'

Set-ItemProperty $token -Name SymbolLevel -Value 'all'
Speak-One 'test_outputs\\sapi5_x64_symbols_all_reload.wav' 'Ala, ma; kota: tak?'

Set-ItemProperty $token -Name NumberMode -Value 'cardinal'
Set-ItemProperty $token -Name SymbolLevel -Value 'most'

Get-ChildItem test_outputs\\sapi5_x64_*_reload.wav | Select-Object Name,Length
