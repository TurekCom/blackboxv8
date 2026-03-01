# Analiza projektu BlackBox V8 (NVDA + C64 ROM)

Data analizy: 2026-02-28

## 1) Co jest w repo

- Sterownik NVDA (aktualnie pakowany): `blackbox_v8_driver.py`
- Starszy silnik + wrapper: `blackbox.py`, `blackbox_nvda.py`
- Stary plik sterownika z błędem składni: `synthDriver.py`
- Skrypt pakujący dodatek NVDA: `repack.py`
- Zasoby C64:
  - `BLACKBOXV8/BBV8 ORYGINAL.BIN` (32 KB, EPROM 27C256)
  - `BLACKBOXV8/FOR 1541U BB8.BIN` (64 KB)
  - `BLACKBOXV8/BLACKBOXV8.CRT` (cartridge CRT, 65 664 B)
  - `SAMPL.T64` (obraz taśmy z programem "SAM POLSKI")

## 2) Stan bieżącej implementacji NVDA

- `blackbox_v8_driver.py` działa i kompiluje się.
- Synteza jest heurystyczna (formant/noise + reguły G2P), inspirowana SAM/BlackBox, ale **nie jest** wykonaniem oryginalnego kodu C64.
- `synthDriver.py` ma błąd składni (`IndentationError`), więc nie nadaje się jako baza.
- Pakowanie dodatku (`repack.py`) kopiuje `blackbox_v8_driver.py` do `build_addon/synthdrivers/blackbox_v8.py`, więc to jest realnie używany sterownik.

## 3) Wynik analizy obrazów C64

- `BLACKBOXV8.CRT`:
  - Sygnatura: `C64 CARTRIDGE`
  - Typ cartridge: `3`
  - Zawiera 4 banki po 16 KB (łącznie 64 KB), mapowane pod `$8000`.
- `FOR 1541U BB8.BIN` jest byte-to-byte zgodny z concatenacją banków z CRT.
- `BBV8 ORYGINAL.BIN` (32 KB) odpowiada zawartości 2 banków z pliku 64 KB (z drobnymi różnicami/patchami między bankami).
- W bankach są klasyczne nagłówki cart C64: wektory startowe + sygnatura `CBM80`.

## 4) Co to oznacza dla celu "wierne brzmienie BlackBox V8"

Samo strojenie obecnego kodu Python nie da pełnej zgodności. Potrzebna jest ścieżka ROM-driven:

1. Uruchomić oryginalny kod 6502 (ROM BlackBox) w emulowanym środowisku C64.
2. Emulować rejestry SID i sposób generacji dźwięku BlackBox.
3. Podawać tekst/komendy jak w oryginale, przechwytywać audio i wystawiać to w NVDA.

To jedyna droga do odtworzenia:
- charakterystycznej ziarnistości,
- timingów i modulacji,
- specyficznych reguł wymowy BlackBox.

## 5) Narzędzia przygotowane

- Zainstalowano `py65` (monitor/emulacja 6502) jako baza do reverse engineeringu ROM.
- Context7 użyto do potwierdzenia aktualnych wymagań NVDA dla SynthDriver (`/nvaccess/nvda`).

## 6) Proponowana architektura docelowa

- `core/rom_adapter.py`:
  - ładowanie banków ROM,
  - mapowanie pamięci/IO wymagane przez kod BlackBox.
- `core/sid_backend.py`:
  - emulacja SID i render PCM,
  - opcjonalne profile: "wierny" / "clean".
- `core/blackbox_runtime.py`:
  - API `speak(text, rate, pitch, volume)` oparte o wykonanie ROM.
- `blackbox_v8_driver.py` (NVDA):
  - tylko integracja kolejki NVDA i sterowanie runtime.

## 7) Najbliższe kroki implementacyjne

1. Zbudować narzędzie reverse (`tools/rom_map.py`) do identyfikacji:
   - wektorów wejścia,
   - tablic fonemów/parametrów,
   - punktów zapisu do SID (`$D400-$D418`).
2. Stworzyć minimalny runtime 6502 + hooki zapisu SID.
3. Dodać tryb testowy generujący WAV dla zestawu fraz referencyjnych.
4. Podmienić w NVDA bieżący silnik heurystyczny na backend runtime.

## 8) Ryzyka

- Największe ryzyko: brak pełnej zgodności timingowej CPU/SID przy uproszczonej emulacji.
- Drugie ryzyko: sterowanie ROM-em może wymagać dokładnej sekwencji inicjalizacji C64 (KERNAL/BASIC hooks).

## 9) Decyzja techniczna

Jeśli celem jest faktycznie "jak dawny syntezator", rekomendowana jest emulacja wykonania ROM + SID.
Obecny silnik formantowy traktować jako tryb awaryjny/fallback.

## 10) Postęp rekonstrukcji (2026-02-28, etap 2)

- Dodano moduł ROM/harness: `blackbox_rom.py`.
- Dodano narzędzie rekonstrukcji: `tools/rom_recon.py`.
- Wykryto statycznie 96 miejsc zapisu do SID w obrazie 64KB (`$D400-$D418`), m.in. sekwencje inicjalizacji przy `PC=$927A`.
- Probe dynamiczny uruchamiany bezpośrednio od `PC=$927A` (bank 0) potwierdził realne zapisy do SID:
  - `$D405/$D40C/$D413/$D406/.../$D418`.

## 11) Postęp rekonstrukcji (2026-02-28, etap 3)

- Dodano bankowaną pamięć C64 w harnessie (`BankedC64Memory`) z obsługą przełączeń banków przez `$DFFF/$DE00`.
- Dodano emulację stanu klawiatury CIA (`$DC01`) jako parametr probe.
- `probe_entry(bank=1, entry=cold)` zaczął przechodzić rzeczywistą ścieżką startową ROM i daje:
  - przełączenia banków: m.in. `$DFFF <= $30`, `$DFFF <= $01`, `$DE00 <= $0F`,
  - zapisy do SID: sekwencja 25 zapisów, od `$D418` do `$D400`.
- Oznacza to, że start cartridge i podstawowy init audio z ROM jest już odtwarzalny w emulacji.

## 12) Postęp rekonstrukcji (2026-02-28, etap 4)

- Dodano analizę xref i bootstrap stubów (`$CA00...`) generowanych dynamicznie przez ROM.
- Potwierdzono, że cold start buduje kod trampolin:
  - `A9 01 8D FF DF 4C DE 80` (switch bank + `JMP $80DE`)
  - `A9 01 8D FF DF 4C B8 82` (switch bank + `JMP $82B8`)
  - `A9 30 8D FF DF 4C 9D E3` (switch bank + `JMP $E39D`)
- Dodano narzędzie `--bootstrap-stubs` w `tools/rom_recon.py` do automatycznego uruchamiania tych stubów po cold starcie.
- Wniosek: przepływ startowy cartridge i mechanizm bank-switch są już uchwycone; kolejnym krokiem jest wejście w ścieżkę parsera tekstu/komend po trampolinach.

## 13) Postęp rekonstrukcji (2026-02-28, etap 5)

- Dodano aktywne hooki KERNAL w harnessie (`CHRIN`, `GETIN`, `CHROUT`, `STOP`) oraz narzędzia:
  - `tools/rom_command_probe.py`
  - `tools/rom_command_sequence.py`
- Potwierdzono działanie dispatchera komend z realnym wyjściem tekstowym:
  - komenda `0x29` zwraca tekst pomocy BLACKBOX przez `CHROUT`.
- Potwierdzono komendę `0x3C` jako twarde wyciszenie SID (25 zapisów zer do `$D400-$D418`).
- Komendy `0x3F` i `0x01` przechodzą do innej ścieżki RAM/bank switch (`bank=1`, nowe PC), ale bez emulacji pełnego KERNAL/BASIC nie dochodzą jeszcze do głównej rutyny mowy.

## 14) Postęp rekonstrukcji (2026-02-28, etap 6)

- Dodano skan klawiszy dispatchera (`tools/rom_command_scan.py`) oparty na klonowaniu stanu po cold-starcie.
- Wynik skanu pełnego `0x00-0xFF`:
  - `0x29` -> ścieżka HELP (`CHROUT`),
  - `0x3C` -> wyciszenie SID (`$D400-$D418 = 0`),
  - `0x3F` i `0x01` -> przejście do ścieżki RAM/bank switch (bank 1), bez finalnego audio.
- Dodano wsparcie keyboard buffer injection (`$C6/$0277`) do probe (`--kbd`), aby zasymulować wejście klawiaturowe C64.
- Eksperyment z periodycznym wywołaniem IRQ-routine (`$82E8`) daje tylko zapisy `$D418`, co potwierdza zależność od ticków systemowych, ale nadal nie uruchamia pełnej syntezy mowy.

## 15) Postęp rekonstrukcji (2026-02-28, etap 7)

- Dodano uproszczony model "live KERNAL" w harnessie:
  - `GETIN/CHRIN` pobierają dane z bufora klawiatury (`$C6/$0277`) jeśli brak danych wejściowych,
  - opcjonalne periodyczne wywołanie IRQ vector z `$0314/$0315` (`--irq-period`).
- Dodano diagnostykę RAM execution hits dla `probe_command_flow`.
- Dla komend `0x3F` i `0x01` z wejściem tekstowym i IRQ:
  - nadal brak zapisów SID toru mowy,
  - ale wykonanie przechodzi intensywnie przez RAM code (`< $0800`) z ~153 unikalnymi adresami (m.in. okolice `$0100..$03A3`), co wskazuje na brakujący fragment emulowanego środowiska systemowego.

## 16) Postęp rekonstrukcji (2026-02-28, etap 8)

- Dodano:
  - start dispatchera od pełnego prologu (`$AF70`, zamiast wejścia w środek pętli),
  - emulację keyscan (`$CB` ładowane z kolejki `$C6/$0277`) opcją `--use-keyscan`,
  - narzędzie hotspotów RAM (`tools/rom_ramtrace.py`).
- Skan pełny `0x00-0xFF` z `--use-keyscan` i IRQ:
  - potwierdza aktywność tylko 4 komend (`0x29`, `0x3C`, `0x3F`, `0x01`),
  - `0x29` = HELP/CHROUT, `0x3C` = SID clear, `0x3F`/`0x01` = wejście do ścieżki RAM/bank1 bez finalnej syntezy.
- Hotspoty RAM dla `0x3F/0x01`:
  - pętle w okolicy `$0338-$0344` oraz `$037B-$0381`,
  - kod jest uruchamiany wielokrotnie, ale nie dochodzi do zapisu formantów SID.

## 17) Postęp rekonstrukcji (2026-02-28, etap 9)

- Rozszerzono narzędzia diagnostyczne:
  - `tools/rom_command_sequence.py`: dodano `--profile` (overlay RAM),
  - `tools/rom_command_scan.py`: dodano `--profile` oraz tryb follow-up (`--prefix-key`), który skanuje klawisze po wejściu do trybu syntezy.
- Dodano nową metodę harnessu: `scan_followup_keys(...)` w `blackbox_rom.py`.
  - Przepływ: cold start -> klawisz wejściowy (np. `0x3F`) -> klonowanie stanu -> skan kolejnych klawiszy.
- Weryfikacja jakości:
  - testy jednostkowe: `7/7 OK` (dodany `test_scan_followup_keys_runs`).
- Wynik kluczowy:
  - skan follow-up dla `0x20-0x7E` po `0x3F` (z i bez profilu `cmd_plus_0900`) daje identyczny efekt:
    - brak zapisów SID (0),
    - brak dodatkowych przełączeń banków,
    - stabilna pętla RAM wokół `$0338-$033A`.
- Wniosek:
  - obecna symulacja klawiatury (`$CB/$C6/$0277`) nie odtwarza jeszcze wejścia oczekiwanego przez tę pętlę.
  - kolejny krok powinien emulować skan matrycy klawiatury C64 przez CIA (`$DC00/$DC01`) zamiast samego bufora KERNAL.
