[![Apri Ultrasuono Atlas](https://img.shields.io/badge/APRI_ULTRASUONO_ATLAS-FD5920?style=for-the-badge&logo=github&logoColor=white)](https://ultrasuono.github.io/ultrasuono-demo/)

# Ultrasuono Atlas

Un paesaggio generativo da esplorare e ascoltare. Spostandoti sulla mappa cambiano temperatura, umidità e luminosità: questi parametri trasformano il logo Ultrasuono e il suono attraverso il motore audio BASIC MODE originale.

La repository contiene soltanto l'atlante. Il terreno nero a terrazze e le curve di livello vengono generati per settori nel browser. Il clima è simulato: non sono osservazioni meteo reali e non vengono chiamate API meteo.

## Provalo

**[Apri l'atlante online](https://ultrasuono.github.io/ultrasuono-demo/)** — non serve installare nulla.

- Tieni premuto sulla mappa nella direzione desiderata oppure usa le frecce della tastiera.
- Seleziona **DEMO** per il suono di prova, **MIC** per il microfono o **FILE** per un file audio locale, quindi premi **PLAY**.
- Regola **DRY/WET** per miscelare suono originale ed elaborato.
- Premi **REC** per registrare l'uscita elaborata e **SAVE** per scaricare la registrazione. Il browser sceglie il formato supportato, per esempio WebM o M4A/MP4. Fermare l'ascolto o cambiare sorgente conclude la registrazione.

Microfono e file vengono elaborati localmente, senza caricamento su server. Il microfono richiede autorizzazione e HTTPS o localhost; usa le cuffie per evitare feedback. I file audio hanno un limite di 50 MB. Il volume di uscita dell'app è fisso al 30%.

Su telefono e tablet verticale i controlli si trovano sotto la mappa; su desktop e tablet orizzontale sono a destra. Il vecchio indirizzo `/atlas/` reindirizza alla stessa applicazione.

## Struttura della repository

- `src/` — interfaccia, terreno, worker, animazione del logo e AudioWorklet.
- `assets/` — grafica originale, font, modello del logo e audio.
- `tests/` — controlli della simulazione, del movimento e del motore audio.
- `scripts/` — server locale, generatore del loop e compilazione WASM.
- `dsp/` — sorgenti C++ e licenze del DSP.
- `atlas/index.html` — compatibilità con il vecchio indirizzo.

## Anteprima locale

Con Node.js installato, dalla cartella della repository:

```sh
node scripts/serve.cjs
```

Apri http://127.0.0.1:8780/. Per ascoltare serve anche `assets/audio/ultrasuono.wasm`, generato dal workflow di pubblicazione: puoi recuperarlo dall'artefatto Pages di GitHub Actions oppure compilarlo dai sorgenti.

## Motore audio e build

Il DSP C++ originale è in `dsp/` e viene eseguito in WebAssembly tramite AudioWorklet. Include gate, comb, flanger, distorsione, filtro di luminosità, delay e riverbero. La temperatura dell'ambiente può raggiungere 50 °C; sopra 20 °C viene rimappata gradualmente nell'intervallo originale del DSP, fino a 40 °C.

Con Emscripten **4.0.17** attivo, dalla radice, in Bash:

```sh
bash scripts/build-dsp.sh
node tests/check-wasm.mjs
```

`scripts/make-demo.mjs` rigenera il loop audio originale incluso; non è una seconda applicazione.

## Verifiche e pubblicazione

```sh
node tests/check.mjs
node tests/check-biomes.mjs
node tests/check-visual-biomes.mjs
node tests/check-tiles.mjs
node tests/check-terraces.mjs
node tests/check-audio-start.mjs
node tests/check-recording.mjs
node tests/check-motion.mjs
```

Il workflow `.github/workflows/pages.yml` esegue questi controlli, verifica il DSP nativo, compila e testa il modulo WASM e pubblica l'atlante su GitHub Pages. I test automatici non sostituiscono l'ascolto e le prove sui dispositivi reali, in particolare su iPhone.

## Asset e licenze

`assets/design/` contiene wordmark, icone SVG originali e riferimento grafico forniti per il progetto; `assets/manifest.json` ne documenta la provenienza. Funnel Display è distribuito con la licenza in `assets/FunnelDisplay-OFL.txt`. Three.js conserva il proprio avviso di licenza in `src/vendor/three.module.js`.

Il DSP applicativo conserva la licenza in `dsp/ULTRASUONO-LICENSE`, attualmente proprietaria. DaisySP e ReverbSc mantengono le licenze in `dsp/vendor/`. I sorgenti DSP e lo script di build vengono distribuiti anche nel sito per permettere la ricompilazione del modulo. La simulazione nel browser non garantisce identità bit per bit con l'hardware Ultrasuono.
