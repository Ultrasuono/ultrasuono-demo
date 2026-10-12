[![Apri la demo Ultrasuono](https://img.shields.io/badge/▶_APRI_LA_DEMO-ULTRASUONO-283b27?style=for-the-badge&labelColor=617746)](https://ultrasuono.github.io/ultrasuono-demo/)

# Ultrasuono — bozza digitale

Pagina statica per la revisione, compatibile con GitHub Pages. Nessuna dipendenza di build, API o backend. Meteo simulato: 500 ambienti deterministici con variazioni continue basate sull'orario. Non rappresenta stazioni o misurazioni reali.

## Anteprima locale

Da questa cartella: `python -m http.server 8765 --bind 127.0.0.1`, poi aprire http://127.0.0.1:8765. Il microfono richiede localhost o HTTPS e autorizzazione del browser. Usare cuffie.

## Audio

Loop originale stereo di 37 secondi incluso (104 BPM: batteria, basso, pad e arpeggi; rigenerabile con `node make-demo.mjs`), microfono/ingresso predefinito del sistema, oppure file locale (limite 50 MB, formati decodificabili dal browser). Stop rilascia il microfono; i file non vengono caricati su un server. Dry/wet e volume funzionano per tutte le sorgenti.

Questa bozza usa una catena Web Audio rappresentativa: filtro luminosità, distorsione caldo, comb freddo, delay e riverbero umidità. Non è il DSP originale: mancano gate multibanda, flanger e le curve/compensazioni esatte. Il porting C++/DaisySP in WebAssembly resta da implementare e verificare. Il font Google è opzionale, con fallback locale.

## Verifica

`node check.mjs` verifica 500 ambienti, intervalli, determinismo e continuità. Il collaudo audio reale richiede un browser e un dispositivo di ascolto.

## GitHub Pages

Copiare il contenuto di questa cartella nella root di una nuova repository pubblica. In Settings → Pages scegliere Deploy from a branch, branch main e cartella / (root). Nessun workflow o servizio esterno necessario.
