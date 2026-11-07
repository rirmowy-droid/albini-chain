# Albini Chain (VST3)

Surowy łańcuch nagraniowy w stylu Steve'a Albiniego, lata 90.:

**Amp → Cab (2 IR + blend) → Room → Console → Tape → Output**

## Pobieranie
**[Najnowsze wydanie](https://github.com/rirmowy-droid/albini-chain/releases/latest)** → `AlbiniChain-Setup.exe` → uruchom.
Wtyczka trafi do `C:\Program Files\Common Files\VST3`; w DAW zrób ponowne skanowanie wtyczek.

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
