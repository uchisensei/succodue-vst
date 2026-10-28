# SUCCO Due 2 v2.0 — Vocal Chain Plugin Suite

**SUCCO Due 2 v2.0** è un plugin VST3 / AU / Standalone per catena vocale modulare, scritto in C++20 con [JUCE 8](https://github.com/juce-framework/JUCE). È un'implementazione **originale** ispirata all'architettura classica di catena vocale (gate → EQ → comp → de-ess → drive → double → chorus → delay → reverb → limit), con estetica UI dark purple + accent giallo, **preset per-modulo** (workflow suite), visualizzatori in tempo reale per ogni modulo, e DSP migliorato (Drive 4 modalità di saturazione).

> ⚠️ Questo progetto è un'opera originale. Non riproduce il codice sorgente, le risorse grafiche o il marchio di alcun plugin commerciale esistente. Il nome "SUCCO", il logo, il layout della UI e tutte le implementazioni DSP sono originali.

---

## 🆕 Novità v2.0

### Per-module preset system (workflow suite)
Ogni modulo (TUNE, GATE, EQ, COMP, DE-ESS, DRIVE, DOUBLE, CHORUS, DELAY, VERB, LIMIT) ha ora il suo **preset dropdown + A/B comparison** nella barra in cima al pannello. I preset vengono salvati in `~/Documents/SUCCO/ModulePresets/<module>/` indipendentemente dal preset globale. Questo è il workflow FabFilter-style — ogni modulo si comporta come un plugin indipendente con la sua libreria di preset.

### Drive 4 modalità di saturazione
Il modulo DRIVE ora ha 4 curve di saturazione blended in modo continuo:
- **Tube** (shape ~0): tanh caldo con leggere armoniche pari
- **Tape** (shape ~0.33): soft clip asimmetrico
- **Transformer** (shape ~0.67): tanh bias-shifted (più armoniche pari)
- **Fuzz** (shape ~1.0): hard clip asimmetrico

### Curve visualizzatori in tempo reale per ogni modulo
- **TUNE**: barra pitch shift
- **GATE**: diagramma envelope (open/closed)
- **EQ**: risposta in frequenza (6 filtri biquad, FabFilter Pro-Q style)
- **COMP**: curva input/output con soft knee + GR meter
- **DE-ESS**: campana alla frequenza crossover
- **DRIVE**: curva del waveshaper (4 mode)
- **DOUBLE**: barre L/R di larghezza stereo
- **CHORUS**: waveform del LFO multi-voce
- **DELAY**: timeline con echo dots
- **VERB**: envelope di decadimento RT60
- **LIMIT**: curva di limitazione + ceiling marker + GR meter

### Yellow accent theme
Knob 3D con drop shadow, track ring giallo, LED dots, output meter segmentato (yellow → orange → red come sui pro compressori/limiter).

---

## Moduli DSP

| # | Modulo    | Parametri principali                                       |
|---|-----------|-------------------------------------------------------------|
| 1 | **Tune**  | Semitoni (-12..+12), Amount (0..100%) — *pitch shifter*    |
| 2 | **Gate**  | Thresh, Range, Attack, Hold, Release                        |
| 3 | **EQ**    | HPF, Low Shelf, 2 Bell, High Shelf, LPF                      |
| 4 | **Comp**  | Thresh, Ratio, Attack, Release, Knee, Makeup               |
| 5 | **De-Ess**| Freq crossover, Thresh, Amount                              |
| 6 | **Drive** | Drive dB, Shape (4 mode), Tone, Output — 2x OS              |
| 7 | **Double**| Amount, Time (ms), Spread                                    |
| 8 | **Chorus**| Rate, Depth, Voices (1..4), Mix                              |
| 9 | **Delay** | Time, Feedback, Ping-Pong, Mix                              |
| 10| **Reverb**| Size, Decay, Damp, Predelay, LowCut, Mix (Freeverb-style)   |
| 11| **Limit** | Ceiling, Release, True-Peak (4x OS)                         |

Master section: **Input / Punch / Mix / Output** (mix dry/wet globale).

---

## Requisiti

### Windows
- Windows 10/11 x64
- CMake ≥ 3.22
- Visual Studio 2022 (workload *Desktop development with C++*)
- Connessione internet (CMake scarica JUCE e VST3 SDK via FetchContent)

### macOS
- macOS 11+ (compatibile anche con Catalina 10.15 grazie a `CMAKE_OSX_DEPLOYMENT_TARGET=10.15`)
- CMake ≥ 3.22
- Xcode Command Line Tools (`xcode-select --install`)

### Linux
- Ubuntu 22.04+ o equivalente
- CMake ≥ 3.22 + Ninja
- Dipendenze dev: ALSA, JACK, sndfile, FreeType2, X11, webkit2gtk-4.1

---

## Build locale

### Windows
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
Output: `build\SUCCO_artefacts\VST3\SUCCO.vst3`

### macOS (universal binary arm64+x86_64)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release --parallel
```

### Installazione

**Windows**: copia `SUCCO.vst3` in `C:\Program Files\Common Files\VST3\`

**macOS**: copia `SUCCO.vst3` in `/Library/Audio/Plug-Ins/VST3/`, poi:
```bash
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/SUCCO.vst3
sudo codesign --force --deep --sign - /Library/Audio/Plug-Ins/VST3/SUCCO.vst3
```

**DAW**: FL Studio → `Options → File → Manage plugins → Find more plugins` → cerca "SUCCO". Reaper lo trova automaticamente dal browser dei plugin.

---

## Build via CI (GitHub Actions)

Il workflow `.github/workflows/build.yml` compila SUCCO su Windows, macOS (universal) e Linux ad ogni push. Per le release taggate:

```bash
git tag v2.0.0
git push origin v2.0.0
```

GitHub creerà automaticamente una release con i tre zip allegati.

---

## Struttura del progetto

```
SUCCO/
├── CMakeLists.txt                  # CMake + JUCE via FetchContent
├── README.md / QUICKSTART.md / UPLOAD_TO_GITHUB.md / LICENSE
├── .github/workflows/build.yml     # CI Windows + macOS + Linux
├── Assets/                         # logo.svg, background.svg
└── Source/
    ├── PluginProcessor.h/.cpp      # AudioProcessor + APVTS
    ├── PluginEditor.h/.cpp         # Editor + ModulePanel
    ├── dsp/                        # 11 DSP modules + ChainProcessor
    ├── gui/
    │   ├── SuccoLookAndFeel.h/.cpp  # Dark purple + yellow theme, 3D knobs
    │   ├── KnobComponent.h/.cpp
    │   ├── ModuleTabBar.h/.cpp
    │   ├── DisplayComponent.h/.cpp   # Logo + big stereo output meter
    │   ├── MeterComponent.h          # Vertical level/GR meter
    │   ├── CurveDisplayComponent.h   # Generic curve display
    │   └── ModulePresetBar.h         # NEW: per-module preset dropdown + A/B
    ├── params/ParameterManager.h/.cpp  # ~60 params via APVTS
    └── presets/
        ├── PresetManager.h/.cpp        # Global chain presets
        └── ModulePresetManager.h       # NEW: per-module preset save/load + A/B
```

---

## Preset

**Factory presets globali (v2.0):**
- `Init` — stato neutro
- `Pop Lead` — catena vocale pop
- `Rap / Trap` — aggressivo
- `Dreamy Lead` — chorus + delay + reverb ampio
- `Podcast Clean` — pulizia per podcast
- `Lo-Fi Hazy` — saturazione + chorus + LPF

**Per-module presets (NEW v2.0):**
- Ogni modulo ha il suo dropdown preset + save/load + A/B
- Salvati in `~/Documents/SUCCO/ModulePresets/<module>/<name>.xml`
- Filtrati per prefisso (es. preset EQ salva solo i parametri `eq_*`)
- A/B comparison in-memory (snapshot istantaneo, niente file)

---

## Limitazioni note

- **Modulo Tune**: pitch shifter base (modulazione di delay crossfade). Pitch-correction su scala richiede engine dedicato (pianificato per v3).
- **Modulo Punch**: knob passivo.
- **Sidechain esterno**: non ancora esposto.
- **Oversampling globale**: solo Drive e Limiter hanno OS locale.
- **Licensing VST3**: il plugin usa lo SDK VST3 di Steinberg distribuito con JUCE.

---

## Licenza

Codice sorgente distribuito sotto licenza **GPL-3.0-or-later** (vedi `LICENSE`).

JUCE: licenza ISC (parte del codice) / GPL-3 (per plugin GPLv3).
VST3 SDK: licenza Steinberg.

---

## Crediti

SUCCO Due 2 v2.0 è stato sviluppato come plugin vocale modulare originale. I concetti DSP sono basati su algoritmi pubblici standard (RBJ biquad, Freeverb, soft-clip tanh, lookahead limiter, pitch shift via crossfade delay). L'estetica UI è dark-purple con accent giallo e knob 3D custom-rendered. I preset per-modulo seguono il workflow FabFilter-style. Nessuna risorsa o codice di plugin commerciali esistenti è stata riprodotta.
