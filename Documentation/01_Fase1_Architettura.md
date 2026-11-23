# TWIN-MU — Compressore/Limiter variable-mu ispirato al Fairchild 670
## Fase 1 — Analisi, architettura e proposta tecnica

> Nome di lavoro: **TWIN-MU** (due canali, variable-mu). Il nome "Fairchild" è un marchio di terzi:
> non va usato nel nome del prodotto né sul pannello. Nella documentazione si può dire
> "ispirato alle caratteristiche sonore del Fairchild 670". Non si dichiara alcuna emulazione circuitale fedele.

---

## 1. Stato dell'ambiente di sviluppo (verificato)

Su questa macchina (Windows 11) **non è installato alcun compilatore C++, né CMake, né Git, né JUCE**
(Python è presente solo come alias del Microsoft Store). Quindi oggi nulla può essere compilato o testato qui.

Toolchain necessaria per Windows (installabile con `winget`):

| Strumento | Uso | Dimensione indicativa |
|---|---|---|
| Visual Studio 2022 Build Tools (workload C++ desktop) | compilatore MSVC, SDK Windows | ~6–8 GB |
| CMake ≥ 3.25 | build system | ~100 MB |
| Git | scaricare JUCE e dipendenze (FetchContent) | ~300 MB |
| JUCE 8.x | framework plugin (scaricato da CMake) | ~150 MB |
| pluginval (opzionale) | validazione automatica del plugin | ~20 MB |

Vincoli di piattaforma che non dipendono da noi:

- **AU** si compila solo su macOS con Xcode. Da questa macchina: no. Soluzione: progetto CMake già pronto per macOS + build su un Mac o su GitHub Actions (runner macOS).
- **AAX** richiede l'AAX SDK di Avid (account sviluppatore) e la firma PACE/iLok per essere caricato in Pro Tools release. Il target CMake viene predisposto e si attiva solo se `AAX_SDK_PATH` è impostato.
- **Standalone + VST3 su Windows**: realizzabili interamente qui, una volta installata la toolchain.

## 2. Scelte tecnologiche

| Area | Scelta | Motivo |
|---|---|---|
| Linguaggio | C++20 | `std::span`, concepts, `constexpr` estesi |
| Framework plugin | JUCE 8 (via CMake FetchContent, versione bloccata) | VST3/AU/AAX/Standalone da un solo codice |
| Motore DSP | **libreria C++ pura `twinmu_dsp`, senza dipendenze da JUCE** | testabile con unit test veloci, riutilizzabile, nessun accoppiamento con la GUI |
| Unit test | doctest (header-only) + CTest | leggero, nessuna dipendenza binaria |
| Validazione plugin | pluginval livello 10 | test di stress, automazione, thread-safety |
| Parametri | `juce::AudioProcessorValueTreeState` | automazione DAW, stato, undo |
| Grafica | `juce::Graphics` vettoriale (niente bitmap) | scalabilità 100–200% e HiDPI nitida |

## 3. Architettura

```
/Source
  /DSP                ← libreria pura C++20 (nessun include JUCE)
     DspMath.h            utilità: dB, flush-denormal, guardia NaN, smoothing
     Filters.h/.cpp       SVF TPT (Cytomic), DC blocker, shelving
     Oversampling.h/.cpp  half-band polyphase IIR 2x/4x/8x (latenza bassa)
     Detector.h/.cpp      peak/RMS, true-peak (FIR 4x)
     GainComputer.h/.cpp  curve: Clean, Variable-Mu, Limiter; knee continuo
     Envelope.h/.cpp      attack/release, rilascio program-dependent a doppia costante
     Compressor.h/.cpp    canale di compressione completo (computer + envelope)
     VintageModel.h/.cpp  topologia feedback variable-mu + 6 time-constant
     Saturation.h/.cpp    stadio valvolare asimmetrico
     Transformer.h/.cpp   saturazione dipendente dalla frequenza + risposta LF/HF
     AnalogNoise.h/.cpp   fruscio + ronzio opzionali
     Sidechain.h/.cpp     sorgente int/ext, filtri HP/LP/BP, sensibilità, mix
     StereoProcessor.h/.cpp  link %, dual mono, M/S, width
     Limiter.h/.cpp       lookahead, true peak, ceiling, soft clip
     Engine.h/.cpp        catena completa, riceve una struct di parametri "piatta"
  /Parameters         ParameterIDs.h, ParameterLayout.cpp, mapping APVTS → Engine
  /PresetManager      PresetManager.cpp, FactoryPresets.cpp, ABState.cpp
  /GUI                LookAndFeel, Knob, VUMeter, GRMeter, CurveView, Scope, Panel
  PluginProcessor.cpp / PluginEditor.cpp
/Tests                test doctest del DSP + test di integrazione del processor
/Presets              preset factory in XML (incorporati come BinaryData)
/Documentation
/CMake
```

### Thread e regole real-time

| Thread | Cosa fa | Cosa **non** fa mai |
|---|---|---|
| Audio (`processBlock`) | legge parametri da `std::atomic<float>`, processa, scrive meter in atomics e campioni dello scope in una FIFO lock-free SPSC | allocare, bloccare mutex, I/O disco, toccare la GUI, log |
| Message/GUI | timer a 60 Hz legge atomics/FIFO, ridisegna; preset su disco; A/B; undo | elaborare audio |

`prepareToPlay` alloca tutto (buffer di oversampling, lookahead, FIFO) per la dimensione massima di blocco
dichiarata dall'host; `processBlock` gestisce comunque blocchi più grandi suddividendoli (alcune DAW non rispettano il massimo dichiarato).
Ogni blocco: `ScopedNoDenormals`, smoothing dei parametri per campione, guardia NaN/Inf sull'uscita (se rileva NaN azzera gli stati e silenzia il blocco invece di propagare).

## 4. Flusso del segnale

```
 Main In ─► Input Gain ─► [Stereo matrix: L/R oppure M/S] ─► Upsample ─┐
                                                                       ▼
 Sidechain: Int/Ext ─► SC Mix ─► SC Filter (HP/LP/BP) ─► Sensitivity ─► Detector A/B
            (Freq-dependent: shelf di enfasi opzionale)                    │  link %
                                                                           ▼
                                   Gain computer (Clean | Variable-Mu) ─► Envelope ─► GR A/B
                                                                           │
   Canale A/B: ─► VCA/Variable-mu gain ─► Tube stage ─► Transformer ─► Makeup A/B
                                                                       │
                         Downsample ◄──────────────────────────────────┘
                              │
          [Matrix inversa M/S→L/R] ─► Width ─► Limiter (lookahead, true peak, ceiling) ─► Soft Clip
                              │
                Mix dry/wet (dry compensato in latenza) ─► Output Gain ─► Noise opz. ─► Out
```

- **SC Listen**: l'uscita diventa il segnale di rilevamento filtrato (per regolare il filtro a orecchio).
- Il dry viene ritardato della stessa latenza del percorso wet (oversampling + lookahead), così il parallel compression non crea filtraggio a pettine.

## 5. Modalità operative e modelli DSP

### 5.1 COMPRESSOR (moderno, trasparente)
- Topologia **feed-forward**, rilevatore peak/RMS selezionabile (RMS con finestra 10 ms).
- Gain computer con knee quadratico continuo (0–24 dB), ratio 1:1…∞:1 (∞ = 1000).
- Envelope nel dominio dB, attack 0.05–200 ms, release 5–5000 ms, release Fixed o Auto (program-dependent).
- Nessuna saturazione salvo quella scelta con TUBE CHARACTER.

### 5.2 LIMITER
- Lookahead 0–5 ms (latenza dichiarata all'host), attacco pari al lookahead con finestra smussata (niente distorsione da attacco istantaneo).
- **True peak**: interpolazione 4x con FIR polifase (stile ITU-R BS.1770-4) sul rilevatore.
- Ceiling −12…0 dBTP, release 1–1000 ms + release adattivo, controllo AMOUNT (blend tra limiting e bypass del limiter).
- Soft clip finale opzionale (polinomiale, oversampled) come rete di sicurezza.

### 5.3 VINTAGE (modello variable-mu dedicato)
Non è un cambio di parametri: è un percorso DSP diverso.
1. **Topologia feedback**: il rilevatore legge l'uscita dello stadio di guadagno (campione precedente), come nei compressori variable-mu, il che dà la tipica compressione "morbida" che cresce progressivamente.
2. **Curva variable-mu**: il guadagno segue la caratteristica di una valvola remote-cutoff,
   `g(Vc) = 1 / (1 + (Vc/V0))^n` — il ratio effettivo **cresce col livello** (≈2:1 vicino alla soglia, fino a ~20:1 su picchi forti). Il controllo RATIO in Vintage regola `n` (la pendenza), THRESHOLD regola la polarizzazione (`V0`).
3. **Sei costanti di tempo** ispirate ai valori pubblicati per il 670:

   | TC | Attacco | Rilascio |
   |---|---|---|
   | 1 | 0.2 ms | 0.3 s |
   | 2 | 0.2 ms | 0.8 s |
   | 3 | 0.4 ms | 2 s |
   | 4 | 0.8 ms | 5 s |
   | 5 | 0.2 ms | 2 s su picchi isolati, 10 s su picchi ripetuti (program-dependent) |
   | 6 | 0.4 ms | 0.3 s picchi isolati, 10 s ripetuti, 25 s livelli costanti (program-dependent) |
   | VAR | attacco e rilascio liberi | |

   Il rilascio program-dependent è implementato come **rete RC a più condensatori**: due/tre stati di inviluppo in parallelo con costanti diverse, caricati in proporzione alla durata dei picchi. Un picco breve carica solo il condensatore veloce (rilascio rapido); una compressione prolungata carica quelli lenti (rilascio lungo, niente "pumping").
4. **Stadio valvolare** dopo il guadagno (vedi 5.4), con livello di pilotaggio dipendente dalla gain reduction (più compressione ⇒ più armoniche, comportamento tipico delle valvole a mu variabile).
5. *(Rinviato)* **Thump/bias leakage**: non incluso nel primo motore DSP; da valutare dopo l'ascolto critico.

### 5.4 Carattere analogico
- **TUBE CHARACTER (0–100%)**: waveshaper asimmetrico con polarizzazione (genera armoniche pari, 2ª dominante), curva `x → (x + b) - k·(x + b)^2…` con limitazione morbida tipo triodo e rimozione DC (DC blocker a 5 Hz). Pilotaggio e bias legati al controllo. Eseguito in oversampling per evitare aliasing.
- **TRANSFORMER**: saturazione dipendente dalla frequenza — il flusso nel nucleo è proporzionale all'integrale della tensione, quindi si satura un segnale passato in un integratore "leaky" (le basse frequenze saturano prima, come nei trasformatori reali), più leggero bump LF (~40 Hz) e roll-off HF (~25 kHz). Isteresi semplificata (memoria a un polo) per la componente di 3ª armonica.
- **ANALOG NOISE**: fruscio rosa a −90…−70 dBFS + ronzio opzionale 50/60 Hz con armoniche; spento di default. Generatore deterministico (xorshift) senza chiamate di sistema.
- **FREQ-DEPENDENT COMPRESSION**: tilt/shelf di enfasi sul solo segnale di rilevamento (−6…+6 dB, 2 kHz di pivot) per far comprimere di più o di meno le basse.

## 6. Canali A/B: Linked, Dual Mono, Mid/Side

L'originale aveva già due canali e il modo **Lateral/Vertical**, che è proprio una matrice M/S: lo sfruttiamo come struttura del prodotto.
Il plugin ha **due strisce di controllo complete, A e B** (Threshold, Ratio, Attack, Release/TC, Makeup, On/Off).

| Modalità | Canale A | Canale B | Rilevamento |
|---|---|---|---|
| **Stereo Linked** | L e R (controlli A) | controlli B nascosti/inattivi | detector misti con **LINK 0–100%**: `d = (1−l)·d_ch + l·max(dL,dR)` |
| **Dual Mono** | Left | Right | indipendenti (link disponibile ma a 0% di default) |
| **Mid/Side** | Mid | Side | indipendenti, on/off per componente |

- **WIDTH 0–200%**: guadagno sul segnale Side applicato con matrice M/S esatta (`M=(L+R)/2`, `S=(L−R)/2`, ricostruzione esatta), nessun filtro, quindi nessuno sfasamento; a 0% il risultato è mono perfetto.
- Un pulsante **"Link A→B"** copia/aggancia i controlli B ai controlli A.

## 7. Sidechain

- Bus sidechain stereo esterno dichiarato nel layout (VST3/AU/AAX lo espongono; se la DAW non lo collega, `External` ricade automaticamente su `Internal` e la GUI lo segnala).
- **SC MIX** 0–100%: miscela segnale interno/esterno.
- **Filtro** SVF TPT (stabile alla modulazione): Off / HP / LP / BP, frequenza 20 Hz–20 kHz, Q 0.3–6.
- **SENSITIVITY** −24…+24 dB: guadagno sul solo rilevamento.
- **SC LISTEN**: monitoraggio del rilevamento.
- Il filtro non tocca mai il percorso audio principale.

## 8. Parametri (ID stabili per l'automazione)

| ID | Nome | Range | Default | Note |
|---|---|---|---|---|
| `mode` | Mode | Compressor / Limiter / Vintage | Vintage | |
| `stereoMode` | Stereo | Linked / Dual Mono / Mid-Side | Linked | |
| `link` | Stereo Link | 0–100 % | 100 | |
| `inGain` | Input | −24…+24 dB | 0 | |
| `thrA`/`thrB` | Threshold | −60…0 dB | −18 | |
| `ratioA`/`ratioB` | Ratio | 1…∞ (skew log) | 4 | ∞ = 1000 |
| `knee` | Knee | 0–24 dB | 6 | 0 = hard |
| `atkA`/`atkB` | Attack | 0.05–200 ms | 10 | log |
| `relA`/`relB` | Release | 5–5000 ms | 250 | log |
| `relMode` | Release Mode | Fixed / Auto / TC1–TC6 | TC2 | TC solo in Vintage |
| `mkA`/`mkB` | Makeup | 0…+24 dB | 0 | |
| `autoMakeup` | Auto Makeup | on/off | off | stima statica della curva |
| `enA`/`enB` | Channel On | on/off | on | |
| `tube` | Tube Character | 0–100 % | 35 | |
| `xfmr` | Transformer | 0–100 % | 30 | |
| `noise` | Analog Noise | off, −90…−70 dB | off | |
| `fdc` | Freq-dep. Comp. | −6…+6 dB | 0 | |
| `scSource` | SC Source | Internal / External | Internal | |
| `scMix` | SC Mix | 0–100 % | 100 | solo con sorgente External: 0 = interno, 100 = solo esterno |
| `scFilter` | SC Filter | Off / HP / LP / BP | HP | |
| `scFreq` | SC Freq | 20–20000 Hz | 80 | log |
| `scQ` | SC Q | 0.3–6 | 0.707 | |
| `scSens` | SC Sensitivity | −24…+24 dB | 0 | |
| `scListen` | SC Listen | on/off | off | non automatizzabile |
| `limOn` | Limiter | on/off | off | attivo in ogni modalità |
| `limCeil` | Ceiling | −12…0 dBTP | −1 | |
| `limRel` | Limiter Release | 1–1000 ms | 50 | |
| `limLook` | Lookahead | 0–5 ms | 1.5 | cambia la latenza |
| `limAmt` | Limit Amount | 0–100 % | 100 | |
| `truePeak` | True Peak | on/off | on | |
| `softClip` | Soft Clip | on/off | off | |
| `width` | Width | 0–200 % | 100 | |
| `mix` | Mix | 0–100 % | 100 | |
| `outGain` | Output | −24…+24 dB | 0 | |
| `os` | Oversampling | 1x / 2x / 4x / 8x | 2x | |
| `eco` | ECO | on/off | off | |
| `bypass` | Bypass | on/off | off | bypass morbido (crossfade 20 ms) |

Ogni parametro continuo è smussato (rampa 20 ms lineare per i guadagni, 50 ms per frequenze di filtro, in scala logaritmica).

## 9. ECO / Live

ECO forza: oversampling 1x, true peak con interpolazione 2x anziché 4x, saturazione con approssimazione polinomiale invece di `tanh`, noise spento, meter aggiornati ogni 2 blocchi. Latenza con ECO e lookahead 0: **0 campioni**.
Latenza dichiarata = ritardo oversampling + lookahead; aggiornata con `setLatencySamples` solo da `prepareToPlay`/fuori dal thread audio.

## 10. Metering

Tutto calcolato nel thread audio in forma economica (picco e somma dei quadrati per blocco), pubblicato in `std::atomic<float>`; balistiche VU (300 ms, 0 VU = −18 dBFS regolabile), peak hold e decadimento calcolati nel thread GUI.
- Input/Output peak + RMS per canale, clip LED con hold 2 s.
- **GR Meter A/B** dal vero valore di guadagno applicato (nessun valore simulato), barra grande + lancetta VU commutabile.
- Curva di trasferimento con il punto di lavoro in tempo reale.
- Oscilloscopio/storico GR scorrevole con evidenziazione dei transienti (FIFO lock-free).

## 11. Preset

- Formato XML (stato APVTS + metadati: nome, categoria, autore, versione schema).
- Factory preset incorporati (sola lettura); user preset in `Documents/TWIN-MU/Presets/<Categoria>/`.
- Salva, Salva come, Rinomina, Elimina, Importa/Esporta (`.tmupreset`), navigazione ◀ ▶, categorie.
- Versione schema per migrare preset vecchi quando si aggiungono parametri.

Factory preset (16): Vocal Smooth Leveler, Vocal Aggressive Rock, Drum Bus Glue, Drum Bus Smash (parallel), Kick Punch, Snare Crack, Bass Even Out, Master Bus Gentle Glue, Master Limiter −1 dBTP, Mix Bus Vintage Warmth, Piano Natural, Strings Slow Swell, Acoustic Guitar Polish, Synth Pad M/S Widen, Live Vocal Safety (ECO), Vintage TC5 Program.

## 12. Funzionalità avanzate (come vengono realizzate)

| Funzione | Implementazione |
|---|---|
| A/B | due snapshot `ValueTree`, swap sul message thread |
| Undo/Redo | `juce::UndoManager` di APVTS, transazioni per gesto |
| Copy/Paste | XML dello stato negli appunti di sistema |
| Randomizer controllato | variazione ±% attorno ai valori attuali, solo parametri non bloccati, intervalli "musicali" |
| Parameter Lock | parametri esclusi dal cambio preset/randomizer (salvati nelle impostazioni, non nel preset) |
| Gain matching I/O | misura LUFS-short a breve termine in/out e regola `outGain` su richiesta (pulsante), non in automatico continuo |
| MIDI Learn | **non nel primo rilascio**: VST3 non riceve MIDI CC come parametri in modo affidabile e le DAW offrono già il mapping MIDI → parametro. Valutabile in seguito per lo Standalone. |

## 13. Proposta grafica

Vedi `02_GUI_Mockup.html`. Principi:
- Pannello rack in alluminio spazzolato scuro, viti agli angoli, due grandi VU illuminati in caldo (ambra) in alto — omaggio al 670 senza copiarne il frontale.
- Gerarchia: **in alto** i meter (GR A/B, curva), **al centro** le due strisce A/B simmetriche con le manopole grandi (Threshold, Ratio) e medie (Attack, Release, Makeup), **a destra** colonna "Character" (Tube, Transformer, Mix, Output), **in basso** barra Sidechain/Limiter/Stereo a schede, **in cima** barra preset/A-B/undo/oversampling/ECO.
- Dimensione base 1100×680, scalabile 100–200% (proporzioni bloccate), tutto vettoriale.
- **Tre temi colore inclusi** (decisione approvata): **Ardesia** (scuro blu-grigio), **Crema anni '60** (frontale avorio), **Verde menta retrò**. Si cambiano dal pulsante ◐ THEME nella barra superiore.
  - Implementazione: una struct `Theme` di colori (pannello, etichette, accenti per sezione A/B/Character/Sidechain) letta dal `LookAndFeel`; cambiare tema = sostituire la struct e `repaint()`, nessun asset bitmap.
  - Il tema è una **preferenza dell'utente**, non un parametro: non è automatizzabile, non viene salvato nei preset né nella sessione DAW, ma nelle impostazioni globali del plugin (`juce::PropertiesFile`), così vale per tutte le istanze.
  - Costanti in ogni tema: display e meter scuri, quadranti VU ambra, colori di sezione coerenti (A, B, Character, Sidechain).
- Interazione: trascinamento verticale, Shift = fine, doppio click = default, click destro = inserimento numerico, rotella, tooltip, valore che appare durante il gesto.

## 14. Rischi tecnici

| Rischio | Impatto | Mitigazione |
|---|---|---|
| Toolchain assente | blocca compilazione e test | installare VS Build Tools + CMake + Git |
| AU/AAX non compilabili qui | formati mancanti | CI macOS su GitHub Actions; AAX con SDK Avid e firma PACE |
| Aliasing della saturazione | distorsione udibile | oversampling + test di aliasing (sinusoide 15 kHz, misura dei prodotti sotto Nyquist) |
| Latenza variabile cambiando lookahead/OS | sfasamenti in DAW | cambio latenza solo con riallocazione, notifica all'host |
| Feedback nel modello Vintage | instabilità | gain computer limitato, stato a un campione, test con transienti estremi e livelli +24 dBFS |
| Sidechain non collegato dalla DAW | rilevamento muto | fallback automatico su interno |
| Prestazioni a 192 kHz × 8x | CPU elevata | limite automatico dell'oversampling a ≤ 384 kHz interni |
| Marchio Fairchild | legale | nome e grafica originali |

## 15. Piano di test

- **Unit DSP** (doctest): ratio statico misurato con sinusoidi stazionarie (errore < 0.1 dB), soglia, knee, attack/release misurati come tempo al 63%, program-dependent (picco singolo vs treno di picchi), limiter mai sopra il ceiling (true peak misurato 4x), link stereo (nessuno spostamento d'immagine), M/S ricostruzione perfetta (errore < −140 dB), width 0% = mono, filtri sidechain (risposta a −3 dB), NaN/Inf in ingresso, silenzio, +24 dBFS, impulso, rumore bianco, segnali fuori fase, tutti i sample rate (44.1–192 kHz) × buffer 1, 32, 64, 128, 256, 512, 4096 e dimensioni irregolari.
- **THD/risposta in frequenza**: sweep e sinusoidi 1 kHz a vari livelli, misura armoniche con DFT; aliasing con sinusoide alta.
- **Integrazione**: salvataggio/ripristino stato, preset round-trip, automazione a rampa veloce (nessun click: limite della derivata dell'uscita), latenza dichiarata = misurata (impulso).
- **pluginval** livello 10; **CPU**: benchmark per blocco a 48 kHz/64 campioni in ogni modalità.
- **DAW**: dipende da quali DAW sono installate qui (Reaper è gratuito da provare e ottimo per i test).

## 16. Roadmap

1. Fase 2 — libreria `twinmu_dsp` + test (compilabile senza JUCE).
2. Fase 3 — GUI vettoriale secondo il mockup.
3. Fase 4 — PluginProcessor, APVTS, preset, metering, A/B, undo.
4. Fase 5 — test, pluginval, profilazione CPU, ascolto.
5. Fase 6 — VST3 + Standalone Windows (installer Inno Setup), CI macOS per AU, AAX se SDK disponibile, manuali.
