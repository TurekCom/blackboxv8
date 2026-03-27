# Release Notes

## NVDA Addon 1.3.0

Asset:
- `blackbox_v8.nvda-addon`

Highlights:
- niezależny sterownik NVDA z natywnym backendem `x86/x64`
- brak wymogu instalacji i rejestracji głosu SAPI5 w systemie
- bazowanie na tym samym rdzeniu SAM/BlackBox co bieżące testy natywne
- dedykowana opcja w ustawieniach głosu NVDA do włączania i wyłączania odczytu emotikon oraz emoji
- pełne polskie nazwy emoji oparte o dane CLDR zamiast krótkiej ręcznej listy
- zgodność deklarowana: minimum NVDA 2021.1, testowane do 2025.3

## SAPI5 0.5.13

Asset:
- `BlackBoxSapi5-0.5.13-dual.exe`

Highlights:
- natywny silnik SAPI5 w C++ (bez wymogu Pythona na komputerze użytkownika)
- instalator dual-arch (x64 + x86 voice registration)
- polska lokalizacja instalatora
- poprawki intonacji (pytania/wykrzyknienia), liczb, znaków i wymowy zbitek
- poprawka błędu z odczytywaniem bookmarków NVDA jako cyfr na końcu wypowiedzi
- dostępne okno konfiguracji z ustawieniami prędkości, wysokości, modulacji i głośności
- nowe ustawienie konfiguratora do włączania i wyłączania odczytu emotikon oraz emoji
- pełne polskie nazwy emoji z assetu CLDR instalowanego razem z DLL
- regulacja parametrów przez token SAPI:
  - `IntonationMode`
  - `IntonationStrength`
  - `VoiceFlavor`
  - `NumberMode`
  - `SymbolLevel`

## Android 0.1.2

Asset:
- `BlackBoxAndroid-0.1.2-release.apk`

Highlights:
- systemowy silnik TTS dla Androida zgodny z ustawieniami zamiany tekstu na mowę
- własne okno ustawień z czarnym tłem i żółtymi napisami
- test odsłuchu, import słownika TXT i sterowanie prędkością, wysokością, głośnością oraz modulacją
- odczyt liter, znaków, emotikon ASCII i pełnego zestawu emoji z polskich danych CLDR
