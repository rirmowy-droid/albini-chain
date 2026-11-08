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

### Suma (źródło „Cały miks”): klej, auto-korektor, wygładzanie góry
Przy źródle „Cały miks” strona **KOMPRESOR I MIKS** pokazuje:
- **Klej sumy** – kompresor na całość: jedna gałka albo ręcznie (próg, ratio, atak, puszczanie – na maksimum „auto”, zależne od programu), **filtr sidechain** (20–300 Hz, domyślnie 100 Hz: stopa i bas nie pompują całością), **mix** (kompresja równoległa), charakter (domyślnie w stylu szyny SSL), automatyczne wyrównanie głośności i wskaźnik.
- **Auto-korektor sumy** – barwa płyty w **27 pasmach (40 Hz – 16 kHz)** z wykresem aktualnej korekty; **ciągłe dopasowanie** (korektor co ~2 s dociąga miks do krzywej płyty, zbiega w ok. 30 s grania). Powyżej 9 kHz korektor przycina normalnie, ale podbija najwyżej o ~2 dB (żeby nie dodać syczenia).
- **Wygładzanie góry** – dynamiczny korektor w 6 pasmach 2,5–14 kHz: ścisza tylko wyskakujące syczenie, ostre talerze i kłujące szczyty (w teście: syczące wybuchy −12 dB, spokojna góra −1,4 dB).

### Brzmienie płyty (auto-miks na sumie)
W stylach z krzywą kopia na sumie (źródło „Cały miks”) dopasowuje miks do płyty w **dwóch przebiegach**:
1. mierzy miks w **27 pasmach co 1/3 oktawy** (40 Hz – 16 kHz), szerokość stereo i głośność, ustawia korektor „barwa płyty” (filtry dopasowane iteracyjnie, żeby nakładające się pasma dały dokładnie zadaną krzywą), panoramę M/S i głośność,
2. słucha jeszcze raz po korekcie i dociąga resztę różnicy; głośność mierzy za limiterem.

Gałka **Barwa płyty** (strona KOMPRESOR I MIKS) ustala siłę dopasowania. Na syntetycznym zespole odchylenie barwy od płyty spada z 3,3–4,6 dB do 0,6–1,6 dB, szerokość trafia z dokładnością do 0,4 dB, głośność do ~1 dB.

Krzywe zmierzone z pełnych plików stereo:

| styl | płyta (utwory) | strona/środek | głośność K |
|---|---|---|---|
| Seattle '91 | Nevermind (Smells Like Teen Spirit, ze składanki z 2002 r.) | −7,4 dB | −14,5 dB |
| Chicago bez litości | In Utero (Scentless Apprentice, Milk It) | −5,2 dB | −12,6 dB |
| Ciepła sala | Ten (Alive, ze składanki) | −8,4 dB | −14,3 dB |
| Smoła | Dirt (Sickman, Down in a Hole) | −8,8 dB | −11,6 dB |
| Czarne słońce | Black Hole Sun (master ze składanki z 2023 r.) | −6,6 dB | −10,4 dB |
| Łazienka w Bostonie | Surfer Rosa (Where Is My Mind?) | −5,2 dB | −19,9 dB |
| Debiut '90 | Facelift (Love, Hate, Love) | −6,2 dB | −14,9 dB |

Konsolety w stylach Łazienka i Debiut '90 dobrane są do brzmienia, nie z dokumentacji sesji.

### Auto-miks
Wstaw wtyczkę na każdą ścieżkę (wybierz źródło) i opcjonalnie na sumę (źródło „Cały miks”). Wybierz styl i kliknij **AUTO-MIKS** w dowolnej kopii, a potem zagraj ~6 s utworu:
1. ścieżki ustawiają studio wg stylu i mierzą głośność oraz widmo (8 pasm),
2. każda ustawia głośność względem bębnów, panoramę (gitary na boki), ustępowanie (bas → stopa, gitary → wokal),
3. **korekta barwy**: każda ścieżka porównuje swoje widmo z widmami pozostałych i robi miejsce tam, gdzie ważniejszy instrument jest głośny (np. gitary oddają dół basowi, a środek-górę wokalowi; wokal oddaje dół), a tam, gdzie sama jest najważniejsza i przykryta – lekko się podbija,
4. suma czeka na wszystkie ścieżki, mierzy całość i ustawia obróbkę oraz głośność końcową.

Style (inspirowane brzmieniem płyt grunge’owych przełomu lat 80. i 90.):
- **Seattle '91** – jasno i zbito (zwarte bębny, gitary szeroko, wokal z przodu)
- **Chicago bez litości** – surowo, dużo sali, mało kompresji (styl Albiniego)
- **Ciepła sala** – przestrzeń i pogłos, cieplej
- **Smoła** – ciemno, ciężko, chorus na gitarach, podwójne głosy
- **Czarne słońce** – gęsto i szeroko, ciężkie gitary
- **Łazienka w Bostonie** – Albini 1987: sucho z bliska, dużo sali, mało kompresji, dynamicznie
- **Debiut '90** – ciężko, raczej sucho, dynamicznie

**COFNIJ** (obok AUTO-MIKS) przywraca we wszystkich kopiach ustawienia sprzed ostatniego auto-miksu.

Działa, gdy program muzyczny trzyma wtyczki w jednym procesie (domyślnie: FL Studio, Reaper, Ableton, Cubase). W aplikacji bez DAW auto-miks ustawia jedno źródło.

**Presety:** 21 – gitara (np. „Garaż w Aberdeen”, „Ściana flaneli”, „Mokry chorus z Seattle”, „Smoła”, „Czarne słońce”), bas, perkusja, wokal i suma.

**Limiter na wyjściu**: limiter z wyprzedzeniem 1,5 ms (sufit −1 dBFS, wykrywanie szczytów między próbkami, release zależny od materiału). Przy +12 dB ponad sufit zniekształcenia ok. 0,1% (stary limiter: 1,6%).

**Oszczędzanie procesora**: gdy kostka Stonogi jest wyłączona i jej ogon wybrzmiał (~3 s), silnik kostki przestaje liczyć. Latencja się nie zmienia.

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

## Piec, mikrofon przy kolumnie, jakość 8x (Stonoga x Albini)

- **Piec**: *Albini* (prosty crunch, jak dotąd), *Brytyjski 800* (w stylu Marshalla JCM800: środek do przodu), *Prostownik* (w stylu Mesa Dual Rectifier: dużo gainu, wycięty środek, zwarty dół), *Amerykański czysty* (w stylu Fendera Twin: dużo zapasu, rwie się końcówka), *Basowy SVT* (w stylu Ampega). Każdy ma kilka stopni lampowych, własną barwę i końcówkę mocy.
- **Presence / Resonance**: ±6 dB góry (3,5 kHz) i dołu (100 Hz) za końcówką, jak w piecach.
- **Sag**: zasilanie „przysiada” przy mocnym uderzeniu (do ok. −2,5 dB po 200 ms), granie robi się bardziej ściśnięte i żywe.
- **Jakość 8x**: kostka i piec liczone 8x zamiast 4x. Aliasy przy mocnym przesterze spadają o ok. 15 dB (gładsza, mniej „cyfrowa” góra), zużycie procesora rośnie ok. 1,7x.
- **Mikrofon przy kolumnie**: *Jak w IR (SM57)*, *SM57*, *MD421*, *Wstęgowy (w stylu R-121)*, *Pojemnościowy (w stylu U87)* – różnica barwy względem SM57, którym zwykle nagrane są IR-y. **Pozycja**: środek membrany = jaśniej, brzeg = ciemniej. **Odległość**: przy siatce = więcej dołu (efekt zbliżeniowy), dalej = mniej dołu i odbicie od podłogi.
- **Wyrównaj fazę A/B**: IR B jest przesuwany (do ±2 ms) i w razie potrzeby odwracany, żeby zgrał się z A – mniej „dziur” przy mieszaniu dwóch kolumn.
- Nowe presety: *Brytyjski piec w Seattle*, *Prostownik, ciężko i zwarcie*, *Twin na krawędzi*, *Wstęgowy przy brzegu, ciemno*, *Bas: Piec SVT w piwnicy*. Stare presety brzmią jak wcześniej (piec *Albini*).

## Magnetofon, atak/wybrzmienie, odsyczanie, własne presety

- **Magnetofon** (studio, obok konsolety): *Ogólny 2"*, *w stylu Studera A827* (czysto, zwarty dół, otwarta góra), *w stylu MCI JH-24* (uderzenie i trochę brudu, parzyste harmoniczne), *w stylu Ampexa MM1200* (ciepło, miękka góra). **Prędkość**: 30 cali/s albo 15 cali/s (garb dołu niżej i mocniej, mniej góry, więcej nasycenia i chwiania).
- **Atak / Wybrzmienie** (na źródle): kształtowanie transjentów niezależne od progu – np. bębny: więcej uderzenia pałki (do +10 dB na początku) albo krótszy/dłuższy ogon (±6 dB).
- **Odsyczanie**: ścisza pasmo nad 4,5 kHz tylko przy głoskach syczących (w teście: „s” −10 dB, samogłoski bez zmian). Wokal ma je domyślnie na 30%.
- **Własne presety**: w menu presetów *Zapisz obecne ustawienia jako preset…* – plik trafia do `Dokumenty\Stonoga x Albini\Presety` i pojawia się w sekcji *Moje presety* (we wszystkich projektach). *Otwórz folder z presetami* pokazuje ten folder.
- Nowe presety: *Perkusja: Atak i zgnieciona sala na taśmie*, *Suma: Ciepła taśma 15 cali*.

## Prawdziwy dubel i środek/boki sumy

- **Prawdziwy dubel (2 podejścia)** – przełącznik w panelu „Na przesterze” (gitara). Prawy kanał dostaje drugie podejście *przed* kostką i piecem: inny rytm (do kilku ms, raz szybciej, raz wolniej), minimalne rozstrojenie, inna siła uderzenia. Przester z obu podejść jest więc różny, jak przy dwóch nagranych ścieżkach. *Panorama* rozsuwa podejścia, *Rozjazd* ustala, jak bardzo się różnią. Preset: *Gitara: Dwa podejścia, ściana*.
- **Środek / boki** (strona 2, źródło „Cały miks”): *Mono dół* (boki bez dołu poniżej 20–250 Hz; 20 = wył., presety sumy: 80 Hz), *Góra boków* (±6 dB przy 5 kHz tylko na bokach), *Środek 2,5k* (±4 dB prezencji w środku – wokal i werbel do przodu/do tyłu).

## IR-y kabinetu

- **Wbudowane IR-y** (Stonoga x Albini): 15 odpowiedzi 4x12 z głośnikami V30 z darmowej paczki Jesters Brutal Pack (CC0), pogrupowane na *ciemne / średnie / jasne*. Wybierasz je z listy przy slocie A i B, a blend miesza oba. Domyślnie gitara: A = średnia 2, B = ciemna 2. Szczegóły i licencja: `Resources/IR/LICENSE-IR.md`.

- **Load IR A / Load IR B**: pliki WAV/AIFF/FLAC. Mono IR, a ze stereo brany jest lewy kanał.
- Ścieżki zapisują się w sesji DAW. Jeśli pliku zabraknie, UI pokaże `missing:`.
- Bez IR-ów działa wbudowana krzywa głośnika, więc wzmacniacz nigdy nie brzmi „fizzy”.
- IR-y **nie są przycinane** (Trim off), żeby dwa mikrofony z tej samej sesji pozostały zgrane w fazie.
  Są normalizowane, więc blend zachowuje poziom.

## Latencja

Około 37 próbek przy 48 kHz (oversampling + transport taśmy). Plugin zgłasza ją hostowi, a ten ją kompensuje.

## Licencja
Wtyczka korzysta z JUCE (https://juce.com) na licencji AGPLv3.
