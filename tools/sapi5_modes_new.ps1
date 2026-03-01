$token='HKLM:\SOFTWARE\Microsoft\Speech\Voices\Tokens\BlackBoxV8.Sapi5'
$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices(); $sel=$null
for($i=0;$i -lt $voices.Count;$i++){ if($voices.Item($i).GetDescription() -like '*BlackBox*'){ $sel=$voices.Item($i); break } }
if($null -eq $sel){ throw 'voice not found' }
$v.Voice = $sel

Set-ItemProperty $token -Name NumberMode -Value 'cardinal'
Set-ItemProperty $token -Name SymbolLevel -Value 'most'
$fs = New-Object -ComObject SAPI.SpFileStream
$fs.Open((Resolve-Path .).Path + '\\test_outputs\\sapi5_x64_cardinal.wav',3,$false)
$v.AudioOutputStream = $fs
$null=$v.Speak('Liczby 123 i 45.',0)
$v.WaitUntilDone(10000)|Out-Null
$fs.Close()

Set-ItemProperty $token -Name NumberMode -Value 'digits'
$fs = New-Object -ComObject SAPI.SpFileStream
$fs.Open((Resolve-Path .).Path + '\\test_outputs\\sapi5_x64_digits.wav',3,$false)
$v.AudioOutputStream = $fs
$null=$v.Speak('Liczby 123 i 45.',0)
$v.WaitUntilDone(10000)|Out-Null
$fs.Close()

Set-ItemProperty $token -Name SymbolLevel -Value 'none'
$fs = New-Object -ComObject SAPI.SpFileStream
$fs.Open((Resolve-Path .).Path + '\\test_outputs\\sapi5_x64_symbols_none.wav',3,$false)
$v.AudioOutputStream = $fs
$null=$v.Speak('Ala, ma; kota: tak?',0)
$v.WaitUntilDone(10000)|Out-Null
$fs.Close()

Set-ItemProperty $token -Name SymbolLevel -Value 'all'
$fs = New-Object -ComObject SAPI.SpFileStream
$fs.Open((Resolve-Path .).Path + '\\test_outputs\\sapi5_x64_symbols_all.wav',3,$false)
$v.AudioOutputStream = $fs
$null=$v.Speak('Ala, ma; kota: tak?',0)
$v.WaitUntilDone(10000)|Out-Null
$fs.Close()

Set-ItemProperty $token -Name NumberMode -Value 'cardinal'
Set-ItemProperty $token -Name SymbolLevel -Value 'most'

Get-ChildItem test_outputs\\sapi5_x64_cardinal.wav,test_outputs\\sapi5_x64_digits.wav,test_outputs\\sapi5_x64_symbols_none.wav,test_outputs\\sapi5_x64_symbols_all.wav | Select-Object Name,Length
