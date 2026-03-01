$v = New-Object -ComObject SAPI.SpVoice
$voices = $v.GetVoices()
Write-Host ('Count=' + $voices.Count)
for($i=0; $i -lt $voices.Count; $i++){
  $t = $voices.Item($i)
  Write-Host ($i.ToString() + ': ' + $t.GetDescription())
}
