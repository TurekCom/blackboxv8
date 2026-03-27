# BlackBox V8

Polski syntezator mowy inspirowany stylem **BlackBox V8 (C64)**, rozwijany w trzech wariantach:

- niezależny dodatek do **NVDA** (`.nvda-addon`, bez wymogu instalacji SAPI5)
- głos **SAPI5** dla Windows 10/11 x64 (z komponentem x86/x64)
- systemowy silnik **Android TTS** (`.apk`) do wyboru w ustawieniach zamiany tekstu na mowę

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

- NVDA addon: `1.3.0`
- SAPI5 installer: `0.5.13`
- Android APK: `0.1.2`

Gotowe artefakty buildów:

- `dist/blackbox_v8.nvda-addon`
- `dist/installer/BlackBoxSapi5-0.5.13-dual.exe`
- `dist/android/BlackBoxAndroid-0.1.2-release.apk`

## Szybki start

### Instalacja NVDA addon

1. Otwórz `dist/blackbox_v8.nvda-addon`.
2. Potwierdź instalację dodatku w NVDA.
3. Zrestartuj NVDA.
4. Wybierz syntezator `BlackBox V8` w ustawieniach NVDA.

Dodatek NVDA zawiera własny natywny backend `x86/x64` i działa niezależnie od rejestracji głosu SAPI5 w systemie.
W ustawieniach głosu NVDA ma też osobny przełącznik odczytu emotikon oraz emoji.

### Instalacja SAPI5

1. Uruchom `dist/installer/BlackBoxSapi5-0.5.13-dual.exe` jako administrator.
2. Po instalacji głos pojawia się jako `BlackBox V8`.
3. Głos jest rejestrowany dla aplikacji SAPI5 x64 i x86.

### Instalacja Android TTS

1. Zainstaluj `dist/android/BlackBoxAndroid-0.1.2-release.apk`.
2. Otwórz aplikację `BlackBox V8`.
3. W ustawieniach Androida wybierz `BlackBox V8` jako silnik zamiany tekstu na mowę.

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
- `build_addon/synthDrivers/bin/x86/blackbox_nvda_native.dll`
- `build_addon/synthDrivers/bin/x64/blackbox_nvda_native.dll`
- `build_addon/synthDrivers/data/emoji/emoji_pl_cldr.tsv`

Skrypt automatycznie:

- buduje natywny backend NVDA w `cpp_core/build-nvda-x86` oraz `cpp_core/build-nvda-x64`,
- pakuje obie DLL do addonu,
- nie wymaga zainstalowanego SAPI5 do działania dodatku.

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

- `dist/installer/BlackBoxSapi5-0.5.13-dual.exe`

### 4) Build Android APK

```powershell
cd android
.\gradlew.bat test assembleRelease
cd ..
```

Wynik:

- `dist/android/BlackBoxAndroid-0.1.2-release.apk`

### 5) Build one-shot (SAPI5 + instalator)

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
- utworzy release `nvda-v1.3.0` z assetem `.nvda-addon`,
- utworzy release `sapi5-v0.5.13` z instalatorem `.exe`,
- utworzy release `android-v0.1.2` z APK Androida.

Ręcznie (alternatywa):

```powershell
git push -u origin main
& 'C:\Program Files\GitHub CLI\gh.exe' release create nvda-v1.3.0 dist/blackbox_v8.nvda-addon --repo turekcom/blackboxv8 --title "NVDA Addon 1.3.0" --notes "Niezależny dodatek NVDA BlackBox V8 (1.3.0) z własnym natywnym backendem x86/x64 oraz pełnym odczytem emoji z CLDR."
& 'C:\Program Files\GitHub CLI\gh.exe' release create sapi5-v0.5.13 dist/installer/BlackBoxSapi5-0.5.13-dual.exe --repo turekcom/blackboxv8 --title "SAPI5 0.5.13" --notes "Wydanie instalatora SAPI5 BlackBox V8 (0.5.13, dual x64/x86) z przełącznikiem odczytu emoji i emotikon."
& 'C:\Program Files\GitHub CLI\gh.exe' release create android-v0.1.2 dist/android/BlackBoxAndroid-0.1.2-release.apk --repo turekcom/blackboxv8 --title "Android 0.1.2" --notes "Silnik BlackBox V8 TTS dla Androida (0.1.2) z pełnym odczytem emoji na podstawie polskich danych CLDR."
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
