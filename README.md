# BlackBox V8

Polski syntezator mowy inspirowany stylem **BlackBox V8 (C64)**, rozwijany w dwóch wariantach:

- dodatek do **NVDA** (`.nvda-addon`)
- głos **SAPI5** dla Windows 10/11 x64 (z komponentem x86/x64)

## Spis treści

- [Aktualne wydania](#aktualne-wydania)
- [Szybki start](#szybki-start)
- [Wymagania](#wymagania)
- [Budowanie projektu](#budowanie-projektu)
- [Testy i weryfikacja](#testy-i-weryfikacja)
- [Publikacja na GitHub Releases](#publikacja-na-github-releases)
- [Struktura repo](#struktura-repo)
- [Licencja](#licencja)

## Aktualne wydania

- NVDA addon: `1.0.14`
- SAPI5 installer: `0.5.1`

Gotowe artefakty buildów:

- `dist/blackbox_v8.nvda-addon`
- `dist/installer/BlackBoxSapi5-0.5.1-dual.exe`

## Szybki start

### Instalacja NVDA addon

1. Otwórz `dist/blackbox_v8.nvda-addon`.
2. Potwierdź instalację dodatku w NVDA.
3. Wybierz syntezator BlackBox V8 w ustawieniach NVDA.

### Instalacja SAPI5

1. Uruchom `dist/installer/BlackBoxSapi5-0.5.1-dual.exe` jako administrator.
2. Po instalacji głos pojawia się jako `BlackBox V8`.
3. Głos jest rejestrowany dla aplikacji SAPI5 x64 i x86.

## Wymagania

- Windows 10 lub nowszy
- Python 3.10+ (do pakowania dodatku NVDA)
- NVDA (do testów dodatku)
- Visual Studio 2022 Build Tools z:
  - MSVC v143
  - CMake tools
- Inno Setup 6 (`iscc`) w `PATH`
- Git i GitHub CLI (`gh`) do publikacji release

## Budowanie projektu

Wszystkie polecenia uruchamiaj w katalogu repo:

```powershell
cd C:\Users\turek\Desktop\blackbox
```

### 1) Build dodatku NVDA (`.nvda-addon`)

```powershell
python package_addon.py
```

Wynik:

- `dist/blackbox_v8.nvda-addon`

### 2) Build silnika SAPI5 (x64 + x86)

```powershell
cmake -S sapi5 -B sapi5/build-x64 -A x64
cmake --build sapi5/build-x64 --config Release

cmake -S sapi5 -B sapi5/build-x86 -A Win32
cmake --build sapi5/build-x86 --config Release
```

Wyniki:

- `sapi5/build-x64/Release/BlackBoxSapi5.dll`
- `sapi5/build-x86/Release/BlackBoxSapi5.dll`

### 3) Build instalatora SAPI5

```powershell
iscc installer/blackbox_sapi5.iss
```

Wynik:

- `dist/installer/BlackBoxSapi5-0.5.1-dual.exe`

### 4) Build one-shot (SAPI5 + instalator)

Skrypt pomocniczy:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build_sapi5_installer.ps1
```

## Testy i weryfikacja

### Test obecności głosu SAPI5 w systemie

```powershell
powershell -ExecutionPolicy Bypass -File tools/check_voices.ps1
```

### Generowanie próbek testowych WAV (x64)

```powershell
powershell -ExecutionPolicy Bypass -File tools/sapi5_focus_test.ps1
powershell -ExecutionPolicy Bypass -File tools/sapi5_slow_focus.ps1
```

Próbki trafiają do katalogu `test_outputs/`.

## Publikacja na GitHub Releases

### Wymagane logowanie GH CLI

```powershell
& 'C:\Program Files\GitHub CLI\gh.exe' auth login
```

### Publikacja repo + release

Automatycznie:

```powershell
powershell -ExecutionPolicy Bypass -File tools/publish_github.ps1 -Owner turekcom -Repo blackboxv8
```

Skrypt:

- utworzy repozytorium (jeśli nie istnieje),
- wypchnie gałąź `main`,
- utworzy release `nvda-v1.0.14` z assetem `.nvda-addon`,
- utworzy release `sapi5-v0.5.1` z instalatorem `.exe`.

Ręcznie (alternatywa):

```powershell
git push -u origin main
& 'C:\Program Files\GitHub CLI\gh.exe' release create nvda-v1.0.14 dist/blackbox_v8.nvda-addon --repo turekcom/blackboxv8 --title "NVDA Addon 1.0.14" --notes "Wydanie dodatku NVDA BlackBox V8 (1.0.14)."
& 'C:\Program Files\GitHub CLI\gh.exe' release create sapi5-v0.5.1 dist/installer/BlackBoxSapi5-0.5.1-dual.exe --repo turekcom/blackboxv8 --title "SAPI5 0.5.1" --notes "Wydanie instalatora SAPI5 BlackBox V8 (0.5.1, dual x64/x86)."
```

## Struktura repo

- `sapi5/` - silnik SAPI5 (C++)
- `installer/` - skrypt Inno Setup
- `BLACKBOXV8/` - pliki referencyjne/archiwalne BlackBox C64
- `dist/` - gotowe buildy i instalatory
- `tools/` - skrypty pomocnicze i testowe
- `test_outputs/` - wygenerowane próbki WAV

## Licencja

- Kod i dokumentacja projektu są udostępnione na licencji **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**.
- Pełny tekst: `LICENSE`
- Link: <https://creativecommons.org/licenses/by-nc-sa/4.0/>

Uwaga:

- Pliki historyczne/binarne/ROM umieszczone w repo mogą podlegać odrębnym prawom autorskim.
- Przed redystrybucją tych materiałów upewnij się, że masz odpowiednie prawa.
