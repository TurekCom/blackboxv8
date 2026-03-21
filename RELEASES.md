# Release Notes

## NVDA Addon 1.2.0

Asset:
- `blackbox_v8.nvda-addon`

Highlights:
- niezależny sterownik NVDA z natywnym backendem `x86/x64`
- brak wymogu instalacji i rejestracji głosu SAPI5 w systemie
- bazowanie na tym samym rdzeniu SAM/BlackBox co bieżące testy natywne
- zgodność deklarowana: minimum NVDA 2021.1, testowane do 2025.3

## SAPI5 0.5.10

Asset:
- `BlackBoxSapi5-0.5.10-dual.exe`

Highlights:
- natywny silnik SAPI5 w C++ (bez wymogu Pythona na komputerze użytkownika)
- instalator dual-arch (x64 + x86 voice registration)
- polska lokalizacja instalatora
- poprawki intonacji (pytania/wykrzyknienia), liczb, znaków i wymowy zbitek
- poprawka błędu z odczytywaniem bookmarków NVDA jako cyfr na końcu wypowiedzi
- dostępne okno konfiguracji z ustawieniami prędkości, wysokości, modulacji i głośności
- regulacja parametrów przez token SAPI:
  - `IntonationMode`
  - `IntonationStrength`
  - `VoiceFlavor`
  - `NumberMode`
  - `SymbolLevel`
