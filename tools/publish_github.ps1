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

gh release create "nvda-v1.2.0" `
    "dist/blackbox_v8.nvda-addon" `
    --repo $repoFull `
    --title "NVDA Addon 1.2.0" `
    --notes "Niezależny dodatek NVDA BlackBox V8 (1.2.0) z własnym natywnym backendem x86/x64."

gh release create "sapi5-v0.5.10" `
    "dist/installer/BlackBoxSapi5-0.5.10-dual.exe" `
    --repo $repoFull `
    --title "SAPI5 0.5.10" `
    --notes "Wydanie instalatora SAPI5 BlackBox V8 (0.5.10, dual x64/x86)."

Write-Host "Publikacja zakonczona: https://github.com/$repoFull"
