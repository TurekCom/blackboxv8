# Release Notes

## NVDA Addon 1.0.14

Asset:
- `blackbox_v8.nvda-addon`

Highlights:
- aktualizacja dodatku NVDA BlackBox V8
- poprawki integracji i stabilności z nowszymi wersjami NVDA
- zgodność deklarowana: minimum NVDA 2021.1, testowane do 2025.3

## SAPI5 0.5.1

Asset:
- `BlackBoxSapi5-0.5.1-dual.exe`

Highlights:
- natywny silnik SAPI5 w C++ (bez wymogu Pythona na komputerze użytkownika)
- instalator dual-arch (x64 + x86 voice registration)
- polska lokalizacja instalatora
- poprawki intonacji (pytania/wykrzyknienia), liczb, znaków i wymowy zbitek
- regulacja parametrów przez token SAPI:
  - `IntonationMode`
  - `IntonationStrength`
  - `VoiceFlavor`
  - `NumberMode`
  - `SymbolLevel`
