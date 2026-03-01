# BlackBox V8

Polski syntezator mowy inspirowany stylem **BlackBox V8 (C64)**, rozwijany w dwóch wariantach:

- dodatek do **NVDA** (`.nvda-addon`)
- głos **SAPI5** dla Windows 10/11 x64 (z komponentem x86/x64)

## Aktualne wydania

- NVDA addon: `1.0.14`
- SAPI5 installer: `0.5.1`

Gotowe artefakty buildów:

- `dist/blackbox_v8.nvda-addon`
- `dist/installer/BlackBoxSapi5-0.5.1-dual.exe`

## Wymagania

- Windows 10 lub nowszy
- NVDA (dla dodatku NVDA)
- Narzędzia build (tylko jeśli kompilujesz ze źródeł):
  - Visual Studio 2022 Build Tools (MSVC + CMake)
  - Inno Setup 6 (`iscc`)

## Instalacja

### NVDA

1. Otwórz `dist/blackbox_v8.nvda-addon`.
2. Potwierdź instalację dodatku w NVDA.
3. Wybierz syntezator BlackBox V8 w ustawieniach NVDA.

### SAPI5

1. Uruchom `dist/installer/BlackBoxSapi5-0.5.1-dual.exe` jako administrator.
2. Po instalacji głos pojawia się jako `BlackBox V8`.
3. Działa w aplikacjach korzystających z SAPI5 (x64 i x86).

## Build ze źródeł

### SAPI5 (x64 + x86)

```powershell
cmake -S sapi5 -B sapi5/build-x64 -A x64
cmake --build sapi5/build-x64 --config Release

cmake -S sapi5 -B sapi5/build-x86 -A Win32
cmake --build sapi5/build-x86 --config Release
```

### Instalator SAPI5

```powershell
iscc installer/blackbox_sapi5.iss
```

Wynik: `dist/installer/BlackBoxSapi5-0.5.1-dual.exe`

## Struktura repo

- `sapi5/` - silnik SAPI5 (C++)
- `installer/` - skrypt Inno Setup
- `dist/` - gotowe buildy i instalatory
- `tools/` - skrypty pomocnicze i testowe
- `test_outputs/` - wygenerowane próbki WAV

## Uwaga o danych i plikach historycznych

Repo zawiera także materiały referencyjne związane z BlackBox/C64.
Przed dalszą redystrybucją upewnij się, że masz prawa do publikacji wszystkich plików binarnych/ROM.

## Licencja

Brak dedykowanego pliku `LICENSE` w tej chwili.
Jeśli chcesz otworzyć projekt publicznie, zalecane jest dodanie licencji (np. MIT).
