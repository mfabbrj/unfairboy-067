# TWIN-MU — Fase 2: motore DSP, rapporto di test

Compilato con MSVC 19.44 (VS 2022 Build Tools), C++20, Release, `/W4 /permissive-`: **0 errori, 0 warning**.
Comando: `.\build.ps1` (configura con Ninja, compila, esegue i test). Il build va in `%LOCALAPPDATA%\TwinMu\build-release`
perché la cartella attuale del progetto ha un percorso troppo lungo per gli strumenti di build.

**Risultato: 33 test, 209 752 verifiche, tutte superate.**

## Moduli realizzati (`Source/DSP`)

| File | Contenuto |
|---|---|
| `DspMath.h` | conversioni dB, sanitizzazione NaN/Inf, coefficienti one-pole (double), smoother lineare, RNG deterministico |
| `Filters.h` | SVF TPT (HP/LP/BP), one-pole, DC blocker |
| `GainComputer.h` | curva clean con knee quadratico, curva variable-mu (ratio crescente con il livello) |
| `Envelope.h` | inviluppo GR a 1–3 "condensatori" paralleli; Fixed, Auto, TC1–TC6 |
| `Saturation.h` | stadio valvolare asimmetrico (armoniche pari), stadio trasformatore (saturazione LF + perdita HF) |
| `AnalogNoise.h` | fruscio rosa + ronzio 50/60 Hz opzionali |
| `Oversampling.*` | half-band FIR 65 tap Kaiser, 1x/2x/4x/8x, latenze intere (0/32/48/56) |
| `Limiter.*` | look-ahead 0–5 ms, true peak 4x, hold + release + media mobile, ceiling garantito sui campioni |
| `Engine.*` | catena completa: input, matrice L/R–M/S, sidechain int/ext + filtri + freq-dependent, link, Compressor/Limiter/Vintage (feedback), oversampling, makeup/auto, width, mix compensato in latenza, output, noise, limiter, soft clip, bypass morbido, meter atomici |

## Misure principali

| Verifica | Risultato |
|---|---|
| Ratio/threshold statici (4:1, 2:1, 10:1) | entro ±0.4 dB dal valore teorico |
| Attacco/rilascio inviluppo | 63.2 % / 36.8 % al tempo nominale (±1 %) |
| TC5 program-dependent | picco isolato: 2.4 s per tornare a 3 dB; compressione prolungata: 11.8 s |
| SVF sidechain | −3.01 dB al taglio (HP/LP), BP a guadagno unitario |
| Oversampling | errore pass-band < −60 dB a 1 kHz; reiezione immagini 2x: 92 dB |
| Aliasing tube 100 % a 15 kHz | 1x: −13 dB → 4x: −90 dB |
| Latenza dichiarata vs misurata (impulso) | identica per 1x/2x/4x/8x |
| Mix 0 %, bypass | uscita = ingresso ritardato (errore < 1e-6) |
| M/S | ricostruzione < −120 dB; Width 0 % = mono esatto; Mid e Side compressi indipendentemente |
| Link | Linked: stessa GR su L/R (< 0.3 dB); Dual Mono: indipendenti |
| Stereo in controfase | resta in controfase (errore < 1e-6) |
| Sidechain esterno / filtro HP | comanda la compressione; HP 1 kHz elimina la GR da un tono a 50 Hz (9.8 dB → 0 dB) |
| Limiter | picchi mai sopra il ceiling, anche con +18 dB di ingresso, a tutte le frequenze di campionamento |
| Robustezza | NaN/Inf in ingresso mai in uscita; silenzio → silenzio; ingressi a +24 dBFS stabili |
| Dimensione dei buffer | uscita identica con blocchi da 1, 32, 37, 64, 128, 256, 4096 campioni |
| Sample rate | 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz × buffer 32–512: stabile e finito |
| CPU (Vintage + limiter, 48 kHz, 64 campioni, un core) | 1x: 115× real time · 2x: 72× · 4x: 43× |

## Note e limiti noti

- **Taratura sonora da fare all'ascolto.** Con Tube al 100 % e segnale a −6 dBFS la 2ª armonica è a −14 dB: tanto colore. Le curve sono corrette, ma le quantità (drive, bias, headroom) si tarano con ascolto critico su materiale reale.
- A 1x la saturazione produce aliasing: il default resta 2x, ECO lo porta a 1x per l'uso dal vivo.
- Il true peak è una stima con interpolazione 4x: il ceiling è garantito sui campioni, quello inter-campione è approssimato.
- Cambiare oversampling o look-ahead cambia la latenza: nel plugin va fatto a elaborazione sospesa (Fase 4).
- Non ancora fatto: wrapper JUCE (VST3/Standalone), GUI, preset, test in DAW e con pluginval.
