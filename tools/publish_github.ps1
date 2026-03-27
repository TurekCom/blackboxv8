param(
    [string]$Owner = "turekcom",
    [string]$Repo = "blackboxv8",
    [switch]$Private
)

$ErrorActionPreference = "Stop"

function Require-Cmd($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Brak narzedzia: $name"
    }
}

Require-Cmd "git"
Require-Cmd "gh"

$repoFull = "$Owner/$Repo"
$visibility = if ($Private) { "--private" } else { "--public" }

git rev-parse --is-inside-work-tree | Out-Null

$hasOrigin = $false
try {
    git remote get-url origin | Out-Null
    $hasOrigin = $true
} catch {}

if (-not $hasOrigin) {
    gh repo create $repoFull $visibility --source . --remote origin --push
} else {
    git push -u origin main
}

gh release create "nvda-v1.3.0" `
    "dist/blackbox_v8.nvda-addon" `
    --repo $repoFull `
    --title "NVDA Addon 1.3.0" `
    --notes "Niezależny dodatek NVDA BlackBox V8 (1.3.0) z własnym natywnym backendem x86/x64 oraz pełnym odczytem emoji i emotikon."

gh release create "sapi5-v0.5.13" `
    "dist/installer/BlackBoxSapi5-0.5.13-dual.exe" `
    --repo $repoFull `
    --title "SAPI5 0.5.13" `
    --notes "Wydanie instalatora SAPI5 BlackBox V8 (0.5.13, dual x64/x86) z przełącznikiem odczytu emoji i emotikon."

gh release create "android-v0.1.2" `
    "dist/android/BlackBoxAndroid-0.1.2-release.apk" `
    --repo $repoFull `
    --title "Android 0.1.2" `
    --notes "Silnik BlackBox V8 TTS dla Androida (0.1.2) z pełnym odczytem emoji i emotikon na podstawie polskich danych CLDR."

Write-Host "Publikacja zakonczona: https://github.com/$repoFull"
