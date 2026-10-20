[![Apri l'Atlante sonoro](https://img.shields.io/badge/▶_APRI_L'ATLANTE-ULTRASUONO-FD5920?style=for-the-badge)](https://ultrasuono.github.io/ultrasuono-demo/atlas/)
[![Apri la demo Ultrasuono](https://img.shields.io/badge/▶_APRI_LA_DEMO-ULTRASUONO-283b27?style=for-the-badge&labelColor=617746)](https://ultrasuono.github.io/ultrasuono-demo/)

# Ultrasuono — bozza digitale

Pagina statica per la revisione, compatibile con GitHub Pages. Nessuna dipendenza di build, API o backend. Meteo simulato: 500 ambienti deterministici con variazioni continue basate sull'orario. Non rappresenta stazioni o misurazioni reali.

## Anteprima locale

Da questa cartella: `python -m http.server 8765 --bind 127.0.0.1`, poi aprire http://127.0.0.1:8765. Il microfono richiede localhost o HTTPS e autorizzazione del browser. Usare cuffie.

## Audio

Loop originale stereo di 37 secondi incluso (104 BPM: batteria, basso, pad e arpeggi; rigenerabile con `node make-demo.mjs`), microfono/ingresso predefinito del sistema, oppure file locale (limite 50 MB, formati decodificabili dal browser). Stop rilascia il microfono; i file non vengono caricati su un server. Dry/wet e volume funzionano per tutte le sorgenti.

Il motore originale C++ è compilato in WebAssembly ed eseguito in AudioWorklet: include gate multibanda, comb, flanger, distorsioni, filtro luminosità, delay e ReverbSc con le curve, smoothing e compensazioni originali. La sorgente è il checkout `firmware-live-optimized`, branch `BASIC-MODE`, base commit `330f43d3ab0d1f41540cd6282fc46ae62f425c5d`; i file AudioProcessor sono copiati senza modifiche dal checkout. I valori simulati sostituiscono i sensori; l'ingresso browser usa il percorso `external_input=true`, registrazione disattivata, guadagno ingresso 1. Il font Google è opzionale, con fallback locale.

L'identità dei sorgenti non garantisce identità bit per bit con ARM o con la parte analogica del dispositivo: compilatore, frequenza effettiva e ingresso audio possono differire. Il firmware originale resta invariato.

I sorgenti applicativi conservano la licenza esistente in `dsp/ULTRASUONO-LICENSE` (attualmente proprietaria); questa pubblicazione non li rilicenzia. DaisySP e ReverbSc mantengono gli avvisi e le licenze in `dsp/vendor`. La distribuzione web include i sorgenti e lo script di compilazione per ricostruire e sostituire il modulo WASM, anche modificando le librerie LGPL.

## Verifica

`node check.mjs` verifica 500 ambienti, intervalli, determinismo e continuità. Il workflow verifica il DSP nativo e il modulo WASM con segnali audio, intervalli meteo e limiti di uscita. Il collaudo audio reale richiede un browser e un dispositivo di ascolto.

Per ricompilare: attivare Emscripten 4.0.17 e lanciare `bash dsp/build.sh` dalla root, poi `node dsp/check-wasm.mjs`. Per l'anteprima locale scaricare anche `ultrasuono.wasm` dall'artefatto GitHub Actions oppure ricompilarlo.

## GitHub Pages

GitHub Pages usa il workflow `.github/workflows/pages.yml`: compila il DSP, esegue i controlli e pubblica la pagina con il modulo WASM. In Settings → Pages scegliere GitHub Actions.
