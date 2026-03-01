# Przygotowanie pod kolejną sesję (SAPI 5, C++)

Data: 2026-02-28

## Co jest gotowe

- Visual Studio 2022 Community jest zainstalowane.
- Kompilator MSVC działa po załadowaniu `VsDevCmd`:
  - `cl` 19.44.35221 (x64)
- CMake jest dostępny:
  - `cmake` 4.2.0
- Ninja jest zainstalowany:
  - `ninja` 1.13.2
- Inno Setup jest zainstalowany i działa z konsoli:
  - `iscc` (Inno Setup 6.7.1)
- Windows SDK zawiera nagłówek SAPI:
  - `C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um\sapi.h`

## Dodane skrypty pomocnicze

- Otwieranie shella z MSVC x64:
  - `tools\vsdevcmd_x64.cmd`
- Szybki check środowiska:
  - `powershell -ExecutionPolicy Bypass -File tools\sapi5_env_check.ps1`

## Jak kompilować instalator z konsoli (Inno Setup)

Tak, da się w pełni z konsoli:

```bat
iscc path\to\installer.iss
```

Przykład z parametrami:

```bat
iscc /Qp /O"dist\installer" /F"BlackBoxSapi5-0.1.0" installer\blackbox_sapi5.iss
```

## Start kolejnej sesji (proponowany workflow)

1. Uruchom `tools\vsdevcmd_x64.cmd`.
2. Przygotuj strukturę projektu COM/SAPI5 (`engine`, `token`, `installer`).
3. Zbuduj DLL silnika (x64) przez CMake + MSVC.
4. Dodaj rejestrację COM i token SAPI5 (HKLM/HKCU, zależnie od trybu instalacji).
5. Dodaj `.iss` i kompilację instalatora przez `iscc`.

