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

### Strona „KOMPRESOR I MIKS”
- **Pełny kompresor źródła**: tryb „Jedna gałka” albo „Ręczny” – próg, ratio, atak, puszczanie, wzmocnienie, **mix** (kompresja równoległa) i wskaźnik ściskania. Charaktery: **Czysty (VCA)**, **Szybki (FET)** (szczytowy, agresywny, lekko brudzi), **Optyczny** (puszcza wolniej, gdy mocno ściska), **Lampowy** (bardzo miękkie kolano, ciepło).
- **Ustępowanie** (kopie wtyczki „słyszą się” nawzajem):
  - **Ustępuj stopie** – bas ścisza się przy każdym uderzeniu stopy (cały albo **tylko dół < 150 Hz**), z regulowanym powrotem,
  - **Ustępuj wokalowi** – gdy wokal śpiewa, gitary robią mu dołek w okolicy 2,5 kHz.
- **Korekta barwy** – 8 pasm (60 Hz–8 kHz) ustawianych przez auto-miks + gałka siły; można poprawiać ręcznie.

### Sprzęt i techniki z lat 90.
- **Model konsolety**: ogólna, **brytyjska** (w stylu dużych Neve: transformatory, najmocniej nasyca dół, parzyste harmoniczne, gruby niski środek), **amerykańska** (w stylu API: twardsze obcinanie, wypchnięta średnica), **czysta in-line** (w stylu SSL / Neotek).
- **Charakter kompresora** (na źródle i osobno na sumie): czysty VCA, w stylu **1176** (ratio 4/8/12/20, atak 0,02–0,8 ms, „wszystkie przyciski”), **LA-2A** (stały atak, dwustopniowe puszczanie), **dbx 160** (twarde kolano, puszczanie 120 dB/s), **vari-mu** (lampowy, ratio rośnie z poziomem), **szyny SSL** (ratio 2/4/10, atak i puszczanie przełączane, auto).
- **Opóźnienie sali** (0–40 ms) – Albini opóźniał mikrofony sali o ok. 10–20 ms.
- **Slap** – krótkie, prawie podwójne echo na wokalu jak na Nevermind.

Na czym oparte są style (to inspiracja, nie kopia brzmienia płyt):
- *Chicago bez litości* – Albini przy In Utero (Pachyderm, konsoleta Neve 8068): kompresja tylko czasem (wokal, limiter typu 1176 na werblu, para dbx 160 na overheadach, delikatnie bas przy nagraniu), **bez kompresji na sumie**; dużo mikrofonów sali, opóźnionych.
- *Seattle '91* – Nevermind (Sound City, Neve 8028 z korektorami 1084): wokal przez kompresor w stylu LA-2A, 1176 na werblu, krótki slap na wokalu, wokale dość suche.
- *Smoła* – Dirt (Dave Jerden): kilka pieców mieszanych pasmami, trzy U87 jako mikrofony sali, phaser i delay na gitarach, analogowa taśma.
- *Czarne słońce* – Superunknown (Bad Animals, preampy Neve, taśma Studer).

### Barwa i szerokość płyty (auto-miks na sumie)
W stylach **Chicago** i **Smoła** kopia na sumie mierzy miks i zbliża go do płyty: barwę w 8 pasmach (60% różnicy, maks. ±4 dB, bez zmiany głośności) i szerokość stereo (panorama M/S powyżej 150 Hz). Krzywe zmierzone z pełnych plików stereo tym samym miernikiem, którego używa wtyczka:

| | 60 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | strona/środek |
|---|---|---|---|---|---|---|---|---|---|
| In Utero (Scentless Apprentice, Milk It) | +3,4 | +6,1 | +1,6 | −1,6 | −1,0 | +0,3 | −2,2 | −6,5 | −5,2 dB (bardzo szeroko) |
| Dirt (Sickman, Down in a Hole) | +1,0 | +1,8 | +1,7 | +1,7 | +1,7 | +1,0 | −2,0 | −6,9 | −8,8 dB (węziej, dół w środku) |

Inne obserwacje: In Utero ma nawet dół rozłożony w stereo (korelacja L/R < 150 Hz ok. 0,65 – mikrofony sali), Dirt ma dół w środku (0,85–0,9). Pozostałe style dostaną krzywe, gdy pojawią się pliki referencyjne.

### Auto-miks
Wstaw wtyczkę na każdą ścieżkę (wybierz źródło) i opcjonalnie na sumę (źródło „Cały miks”). Wybierz styl i kliknij **AUTO-MIKS** w dowolnej kopii, a potem zagraj ~6 s utworu:
1. ścieżki ustawiają studio wg stylu i mierzą głośność oraz widmo (8 pasm),
2. każda ustawia głośność względem bębnów, panoramę (gitary na boki), ustępowanie (bas → stopa, gitary → wokal),
3. **korekta barwy**: każda ścieżka porównuje swoje widmo z widmami pozostałych i robi miejsce tam, gdzie ważniejszy instrument jest głośny (np. gitary oddają dół basowi, a środek-górę wokalowi; wokal oddaje dół), a tam, gdzie sama jest najważniejsza i przykryta – lekko się podbija,
4. suma czeka na wszystkie ścieżki, mierzy całość i ustawia obróbkę oraz głośność końcową.

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
