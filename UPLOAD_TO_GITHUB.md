# 📤 Guida — Caricare SUCCO su GitHub manualmente (senza riga di comando)

Hai 3 opzioni, dalla più facile alla più manuale:

| Metodo | Difficoltà | Pro | Contro |
|--------|------------|-----|--------|
| **1. GitHub Desktop** | ⭐ Facilissimo | Grafico, gestisce tutto lui | Da installare una volta |
| **2. Web upload con ZIP** | ⭐⭐ Medio | Niente installazioni | 100 file max, attenzione alle cartelle |
| **3. Trascina file in browser** | ⭐⭐⭐ Più faticoso | Zero install | Noioso con 38 file, manualmente |

---

## ⭐ Metodo 1 — GitHub Desktop (CONSIGLIATO)

### Step 1 — Crea il repo su GitHub (1 min)
1. Vai su https://github.com/new
2. **Repository name**: `succo-vst`
3. Scegli **Public** (per le Actions gratuite)
4. **NON** aggiungere README/gitignore
5. Click **Create repository**
6. Non fare altro — chiudi la pagina

### Step 2 — Installa GitHub Desktop (3 min)
1. Vai su https://desktop.github.com/
2. Scarica e installa (Windows o Mac)
3. Apri GitHub Desktop
4. Sign in con il tuo account GitHub

### Step 3 — Aggiungi la cartella `succo/` (2 min)
In GitHub Desktop:
1. Menu in alto a sinistra → **Add** → **Add existing repository**
2. Sfoglia fino a trovare la tua cartella `succo`
3. Click **Choose**
4. Se chiede "Create a repository here", inserisci:
   - Name: `succo-vst`
   - Description: `SUCCO Vocal Chain VST3`
   - Spunta **Initialize this repository with a README**? → NON spuntare (abbiamo già i file)
5. Click **Create repository**

### Step 4 — Pubblica su GitHub (1 min)
1. In alto al centro troverai un pulsante viola **Publish repository**
2. Spunta **Keep this code private** → NON spuntare (vuoi Public per le Actions)
3. Click **Publish repository**
4. Attendi ~30 sec — sta caricando i 38 file

### Step 5 — Verifica
- Vai su https://github.com/TUO-USERNAME/succo-vst
- Dovresti vedere tutti i file del progetto nella struttura corretta:
  ```
  succo-vst/
  ├── CMakeLists.txt
  ├── README.md
  ├── QUICKSTART.md
  ├── Source/         ← cartella con tutti i .h/.cpp
  ├── Assets/
  └── .github/workflows/build.yml
  ```

### Step 6 — Attendi la build CI (15-20 min)
- Tab **Actions** in alto → vedi 3 job: Windows, macOS-Universal, Linux
- Quando diventano ✅ verdi, sei a cavallo

### Step 7 — Scarica il VST3 (1 min)
1. Clicca sull'ultimo run "Build SUCCO VST3"
2. Scorri in fondo → **Artifacts**
3. Scarica `SUCCO-Windows.zip` o `SUCCO-macOS-Universal.zip`
4. Decomprimi → cartella `VST3/SUCCO.vst3`

---

## 🌐 Metodo 2 — Web upload con ZIP (alternativa)

Usa questo solo se non vuoi installare GitHub Desktop.

### Step 1 — Crea il repo (1 min)
- Vai su https://github.com/new
- Name: `succo-vst`
- Scegli **Public**
- **Spunta** "Add a README file" ← IMPORTANTE (crea il repo non vuoto)
- Click **Create repository**

### Step 2 — Trascina i file (5-10 min)
⚠️ **Limitazione importante**: GitHub web accetta max 100 file per upload. Il progetto ne ha ~38, quindi sei sotto il limite.

1. Apri il tuo file manager e vai nella cartella `succo`
2. **Devi trascinare i file MANTENENDO la struttura delle cartelle**
3. Su GitHub, nel tuo repo, click **Add file → Upload files**
4. Trascina **tutto il contenuto** della cartella `succo/` nella zona tratteggiata:
   - `CMakeLists.txt`
   - `README.md`
   - `QUICKSTART.md`
   - `LICENSE`
   - `.gitignore`
   - **Tutta la cartella `Source/`** (con sottocartelle `dsp/`, `gui/`, `params/`, `presets/`)
   - **Tutta la cartella `Assets/`**
   - **Tutta la cartella `.github/`** (attenzione: cartella nascosta su Mac/Linux, vedi sotto)
5. Verifica che GitHub mostri i file nelle sottocartelle giuste:
   ```
   succo-vst/
   ├── CMakeLists.txt ✓
   ├── README.md ✓
   ├── Source/PluginProcessor.cpp ✓
   ├── Source/dsp/Gate.h ✓
   ├── .github/workflows/build.yml ✓
   ```
6. In fondo, "Commit changes" → messaggio `Initial commit` → click **Commit changes**

### ⚠️ Cartelle nascoste (.github)
- **Windows**: abilita "Elementi nascosti" in Esplora Risorse → Visualizza
- **macOS**: `Cmd + Shift + .` in Finder per mostrare file/cartelle nascoste
- La cartella `.github/` contiene il workflow CI — **SE NON LA CARICHI LA CI NON PARTE!**

### Step 3 — Verifica struttura
Nel tuo repo su GitHub, devi vedere:
```
.github/workflows/build.yml  ← se manca questo, la CI non partirà
Source/dsp/Gate.h            ← se manca la cartella dsp/, la build fallisce
Source/gui/SuccoLookAndFeel.h
...
```

### Step 4 — Attendi CI (15-20 min) e scarica l'artefatto
Come il Metodo 1, step 6 e 7.

---

## 🚫 Metodo 3 — Drag-drop puro in browser (sconsigliato)

Identico al Metodo 2 ma invece di trascinare tutto in una volta, trascini un file per volta.
Lo sconsiglio: con 38 file ci metti 20 minuti solo per l'upload.

---

## ⚠️ Errori comuni & soluzioni

| Problema | Soluzione |
|----------|-----------|
| "Repository exceeds size limit" | Hai caricato anche la cartella `build/`? Eliminala dal upload |
| La CI non parte (nessun job nella tab Actions) | Verifica che `.github/workflows/build.yml` sia caricato con la struttura giusta |
| La CI parte ma fallisce con "file not found" | Verifica che tutte le sottocartelle `Source/dsp/`, `Source/gui/`, etc. siano presenti |
| GitHub non accetta cartelle vuote | Non ce ne sono nel progetto, non dovresti avere problemi |
| Errore su CMake nella CI "Could not find JUCE" | Normale al primo run, FetchContent scarica JUCE — attendi |
| "Uncommitted changes" in GitHub Desktop | Click su **Fetch origin** → poi **Pull** se richiesto |

---

## 🎯 Quale scegliere?

- **Prima volta su GitHub?** → Metodo 1 (GitHub Desktop). Massimo 6 minuti di setup, tutto grafico.
- **Hai già usato GitHub via web?** → Metodo 2 (Web upload). Attento solo alla struttura cartelle.
- **Vuoi il modo più veloce in assoluto?** → Metodo 1 (GitHub Desktop fa l'upload dei 38 file in un solo click, mentre il web richiede drag-drop manuale).

---

## ✅ Verifica finale

Dopo l'upload, nel tuo repo deve esserci questo albero esatto (usa la vista "Files" su GitHub):
```
succo-vst/
├── .github/workflows/build.yml     ← CRITICO per CI
├── .gitignore
├── Assets/
│   ├── background.svg
│   └── logo.svg
├── CMakeLists.txt
├── LICENSE
├── QUICKSTART.md
├── README.md
└── Source/
    ├── PluginEditor.cpp
    ├── PluginEditor.h
    ├── PluginProcessor.cpp
    ├── PluginProcessor.h
    ├── dsp/
    │   ├── ChainProcessor.cpp
    │   ├── ChainProcessor.h
    │   ├── Chorus.h
    │   ├── Compressor.h
    │   ├── DeEsser.h
    │   ├── Delay.h
    │   ├── Doubler.h
    │   ├── Drive.h
    │   ├── EQ.h
    │   ├── Gate.h
    │   ├── Limiter.h
    │   ├── Oversampler.h
    │   ├── PitchShift.h
    │   └── Reverb.h
    ├── gui/
    │   ├── DisplayComponent.cpp
    │   ├── DisplayComponent.h
    │   ├── KnobComponent.cpp
    │   ├── KnobComponent.h
    │   ├── ModuleTabBar.cpp
    │   ├── ModuleTabBar.h
    │   ├── SuccoLookAndFeel.cpp
    │   └── SuccoLookAndFeel.h
    ├── params/
    │   ├── ParameterManager.cpp
    │   └── ParameterManager.h
    └── presets/
        ├── PresetManager.cpp
        └── PresetManager.h
```

Totale: **38 file**. Sotto il limite di 100 di GitHub web upload.

---

## 💬 Aiuto diretto

Se durante la procedura ti blocchi, dimmi:
1. Quale metodo hai scelto (1, 2 o 3)
2. Su quale step sei
3. L'errore esatto che vedi

Ti guido passo-passo.
