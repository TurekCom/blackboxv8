# BlackBox Android

Projekt Android TTS dla BlackBox V8.

## Co zawiera
- usługę `TextToSpeechService`, którą Android może wybrać jako silnik TTS,
- ekran ustawień z suwakami prędkości, wysokości, głośności i modulacji,
- import słownika TXT przez systemowy wybieracz pliku,
- JNI do wspólnego rdzenia C++ z repozytorium.

## Build
1. Doinstaluj w SDK: `platforms;android-36`, `build-tools;36.1.0`, `cmake;3.31.6`, `ndk;28.2.13676358`.
2. Wejdź do katalogu `android`.
3. Uruchom `gradlew.bat assembleDebug`.

## Słownik
Format pliku:
`facebook=fejsbuk`
`gmail=dżimejl`

Jedna para na linię.
