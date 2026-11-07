# Albini Chain (VST3) + Stonoga x Albini

Surowy łańcuch nagraniowy w stylu Steve'a Albiniego, lata 90.:

**Amp → Cab (2 IR + blend) → Room → Console → Tape → Output**

## Pobieranie
**[Najnowsze wydanie](https://github.com/rirmowy-droid/albini-chain/releases/latest)** → `AlbiniChain-Setup.exe` → uruchom.
Instalator wgrywa:
- **Albini Chain** (VST3) – sam łańcuch nagraniowy,
- **Stonoga x Albini** (VST3) – kostka Stonoga + cały łańcuch w jednej wtyczce,
- **Stonoga x Albini (bez DAW)** – ta sama wtyczka jako aplikacja w menu Start (gitara przez interfejs audio).

W DAW zrób ponowne skanowanie wtyczek.

## Stonoga x Albini 2

Kanał studyjny w stylu Albiniego dla **gitary, basu, perkusji, wokalu i całego miksu**, z kostką Stonoga i auto-miksem.

**Tor:** [gitara/bas: kostka → wzmacniacz → kolumna (2 IR), bas + czysty dół z linii] → barwa (mikrofon, źródło) →
**na źródle:** kompresja → chorus → panorama (dwa podejścia) → pogłos → **studio:** sala (+ zgniecenie) → konsoleta → taśma →
**na całości:** kompresja → panorama M/S → pogłos → głośność, pan → ochrona uszu

- **Źródło**: Gitara, Bas, Perkusja, Wokal, Cały miks. Każde ma własne brzmienie bazowe; kostka i wzmacniacz działają przy gitarze i basie.
- **Mikrofon**: dynamiczny (prezencja), wstęgowy (ciemno, gładko), pojemnościowy (powietrze, pełnia).
- **Chorus**: 6 głosów rozłożonych od lewej do prawej, każdy z własnym tempem i dryfem; dół (< 180 Hz) zostaje w środku.
- **Panorama źródła**: dwa nagrane podejścia – lewo oryginał, prawo drugie podejście; **Rozjazd** = jak bardzo się różnią.
- **Pogłos źródła** z regulowaną **długością** (ok. 0,8–5,6 s), osobny od pogłosu całości.
- **Sala** z **zgnieceniem** (mocno skompresowane mikrofony sali, klasyka bębnów), **barwa** ciemno/jasno, **obcięcie dołu**, **miejsce na wokal**.

### Auto-miks
Wstaw wtyczkę na każdą ścieżkę (wybierz źródło) i opcjonalnie na sumę (źródło „Cały miks”). Wybierz styl i kliknij **AUTO-MIKS** w dowolnej kopii, a potem zagraj ~6 s utworu:
ścieżki ustawiają studio wg stylu, głośność względem bębnów, gitary rozstawiają się na boki, gitary dostają obcięcie dołu pod bas i miejsce na wokal; suma czeka na ścieżki, mierzy całość i ustawia głośność końcową.

Style (inspirowane brzmieniem płyt grunge’owych początku lat 90.):
- **Seattle '91** – jasno i zbito (zwarte bębny, gitary szeroko, wokal z przodu)
- **Chicago bez litości** – surowo, dużo sali, mało kompresji (styl Albiniego)
- **Ciepła sala** – przestrzeń i pogłos, cieplej
- **Smoła** – ciemno, ciężko, chorus na gitarach, podwójne głosy
- **Czarne słońce** – gęsto i szeroko, ciężkie gitary

Działa, gdy program muzyczny trzyma wtyczki w jednym procesie (domyślnie: FL Studio, Reaper, Ableton, Cubase). W aplikacji bez DAW auto-miks ustawia jedno źródło.

**Presety:** 21 – gitara (np. „Garaż w Aberdeen”, „Ściana flaneli”, „Mokry chorus z Seattle”, „Smoła”, „Czarne słońce”), bas, perkusja, wokal i suma.

Szumowe efekty i stary chór Stonogi przejdą do osobnego efektu (kod w `Source/Later/`).

## Build

- **GitHub Actions**: każdy push na `main`/`master` buduje VST3 dla Windows (artefakt `AlbiniChain-Windows-VST3`).
- **Lokalnie** (Windows, VS 2022 lub nowszy):
  ```
  cmake -S . -B build -A x64
  cmake --build build --config Release
  ```
  JUCE 8.0.4 jest brany z `./JUCE`, a jeśli go tam nie ma, CMake pobiera go sam.

## Gałki

| Gałka   | Co robi |
|---------|---------|
| Input   | gain przed wzmacniaczem (-12…+24 dB) |
| Drive   | od czystego do crunchu; asymetryczny soft clip, 4x oversampling |
| Cab A/B | blend dwóch IR-ów (liniowy, jak dwa suwaki mikrofonów) |
| Room    | ilość mikrofonów ambientowych (równolegle do sygnału) |
| Size    | wielkość pomieszczenia, T60 ok. 0,2–1,2 s |
| Console | nasycenie transformatora, mocniejsze na dole pasma |
| Tape    | kompresja taśmy, head bump, utrata góry, delikatny wow/flutter |
| Output  | gain wyjściowy |

## IR-y kabinetu

- **Load IR A / Load IR B**: pliki WAV/AIFF/FLAC. Mono IR, a ze stereo brany jest lewy kanał.
- Ścieżki zapisują się w sesji DAW. Jeśli pliku zabraknie, UI pokaże `missing:`.
- Bez IR-ów działa wbudowana krzywa głośnika, więc wzmacniacz nigdy nie brzmi „fizzy”.
- IR-y **nie są przycinane** (Trim off), żeby dwa mikrofony z tej samej sesji pozostały zgrane w fazie.
  Są normalizowane, więc blend zachowuje poziom.

## Latencja

Około 37 próbek przy 48 kHz (oversampling + transport taśmy). Plugin zgłasza ją hostowi, a ten ją kompensuje.

## Licencja
Wtyczka korzysta z JUCE (https://juce.com) na licencji AGPLv3.
