# 🚀 Guida Rapida — Hai già il progetto in `succo/`

Il progetto è scaricato sul tuo computer in una cartella chiamata `succo`.
Da lì, hai **due strade** per ottenere il `SUCCO.vst3`:

| Percorso | Quando conviene | Tempo |
|----------|-----------------|-------|
| **A — Build locale** | Hai Visual Studio (Win) o Xcode (Mac) | ~15 min |
| **B — GitHub Actions** | Non vuoi installare dev tools | ~25 min |

---

## 🔧 Percorso A — Build locale (PIÙ VELOCE se hai gli strumenti)

### Su Windows

**Prerequisiti (una volta sola, ~10 min):**
- [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) → installa con il workload "Desktop development with C++"
- [CMake](https://cmake.org/download/) → durante l'installazione spunta "Add CMake to system PATH"

**Build (5-10 min):**
Apri **PowerShell** e:
```powershell
cd C:\percorso\al\tuo\succo
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

> La prima volta CMake scarica JUCE (~400 MB, ci vuole pazienza al primo run).

**Risultato:**
```
succo\build\SUCCO_artifacts\VST3\SUCCO.vst3
```

### Su macOS

**Prerequisiti (una volta sola, ~3 min):**
```bash
xcode-select --install
brew install cmake
```

**Build (5-10 min):**
```bash
cd ~/percorso/al/tuo/succo
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --parallel
```

**Risultato:**
```
succo/build/SUCCO_artifacts/VST3/SUCCO.vst3
```

---

## ☁️ Percorso B — GitHub Actions (zero install locale)

### Step 1 — Crea un repo su GitHub (2 min)
1. Vai su https://github.com/new
2. Repository name: `succo-vst`
3. Scegli **Public** (le Actions gratuite funzionano meglio)
4. **NON** spuntare "Add README/gitignore" (li abbiamo già)
5. Click **Create repository**

### Step 2 — Pusha il progetto dalla tua cartella `succo/` (3 min)

**Windows (PowerShell):**
```powershell
cd C:\percorso\al\tuo\succo
git init
git add .
git commit -m "Initial commit - SUCCO vocal chain"
git branch -M main
git remote add origin https://github.com/TUO-USERNAME/succo-vst.git
git push -u origin main
```

**macOS/Linux:**
```bash
cd ~/percorso/al/tuo/succo
git init
git add .
git commit -m "Initial commit - SUCCO vocal chain"
git branch -M main
git remote add origin https://github.com/TUO-USERNAME/succo-vst.git
git push -u origin main
```

> Se non hai git: Win → https://git-scm.com/download/win ; Mac → `brew install git`

### Step 3 — Attendi la build CI (15-20 min)
- Vai su https://github.com/TUO-USERNAME/succo-vst/actions
- Vedrai 3 job paralleli: **Windows**, **macOS-Universal**, **Linux**
- Primo build: JUCE download ~5 min, compile ~10 min
- Quando vedi ✅ verde, è pronto

### Step 4 — Scarica il binario (2 min)
1. Click sull'ultimo run completato
2. Scorri fino in fondo → sezione **Artifacts**
3. Scarica:
   - `SUCCO-Windows.zip` (se su Windows)
   - `SUCCO-macOS-Universal.zip` (se su Mac)
4. Decomprimi → cartella `VST3/SUCCO.vst3`

---

## 📥 Installazione finale in FL Studio / Reaper

### Windows
Copia `SUCCO.vst3` in:
```
C:\Program Files\Common Files\VST3\
```

### macOS
Copia `SUCCO.vst3` in:
```
/Library/Audio/Plug-Ins/VST3/     (tutti gli utenti - richiede `sudo`)
~/Library/Audio/Plug-Ins/VST3/    (solo il tuo utente)
```

**⚠️ Mac — Sblocca il Gatekeeper (importante!):**
```bash
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/SUCCO.vst3
sudo codesign --force --deep --sign - /Library/Audio/Plug-Ins/VST3/SUCCO.vst3
```

### Apri la DAW
- **FL Studio**: `Options → File → Manage plugins → Find more plugins` → cerca "SUCCO"
- **Reaper**: il browser dei plugin in automatico trova "SUCCO" → trascinalo su una traccia

---

## 🐛 Problemi comuni & soluzioni veloci

| Problema | Soluzione |
|----------|-----------|
| `git` non riconosciuto (Win) | Installa https://git-scm.com/download/win |
| `cmake` non riconosciuto | Reinstalla CMake spuntando "Add to PATH" |
| CMake non trova Visual Studio | Reinstalla VS 2022 con workload "Desktop development with C++" |
| Primo build lentissimo | Normale, sta scaricando JUCE (~400 MB). Dal secondo build va in cache |
| FL Studio non vede il plugin | Verifica percorso: `C:\Program Files\Common Files\VST3\` |
| Reaper non vede il plugin | `Options → Preferences → Plug-ins → VST → Re-scan` |
| Mac blocca il plugin | Esegui i 2 comandi `xattr` + `codesign` sopra |
| Build CI fallisce | Apri il log del job → cerca l'errore rosso → in genere è un problema CMake |

---

## ⏱️ Tabella dei tempi

| Step | Win | Mac |
|------|-----|-----|
| Prerequisiti (una volta) | 10 min | 3 min |
| CMake configure (scarica JUCE) | 5-10 min | 5-10 min |
| Build | 3-5 min | 3-5 min |
| Install plugin | 2 min | 2 min |
| **TOTALE (build locale)** | **~20 min** | **~13 min** |
| **TOTALE (via GitHub CI)** | **~25 min** | **~25 min** |

---

## 💡 Quale scegliere?

- **Hai Visual Studio o Xcode già installati?** → Percorso A (build locale), è il più veloce
- **Non sai cosa siano / non vuoi installarli?** → Percorso B (GitHub CI), più semplice ma leggermente più lento
- **Vuoi una release stabile da condividere?** → Percorso B + tag `v1.0.0`:
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```
  → release automatica su https://github.com/TUO-USERNAME/succo-vst/releases

---

**Domanda di percorso**: dimmi il tuo sistema operativo e se hai già Visual Studio (Win) o Xcode CLI tools (Mac), ti do i comandi esatti personalizzati.
