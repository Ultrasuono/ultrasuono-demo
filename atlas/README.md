# Atlante sonoro Ultrasuono

Versione approvata del prototipo: mappa generativa navigabile, terreno a terrazze, foreste e deserti, montagne calde e fredde, praterie secche e foreste tropicali. Nuvole ed esposizione variano la luce; temperatura, umidità e luce controllano il logo e il DSP audio originale.

## Aprire in locale

Con Node.js installato, da questa cartella:

```sh
node serve.cjs
```

Aprire http://127.0.0.1:8780/. Il microfono richiede localhost o HTTPS. Audio demo e file personali sono elaborati nel browser.

## Controlli

Tenere premuto sulla mappa verso la destinazione oppure usare le frecce. Scegliere demo, microfono o file e premere Avvia ascolto.

## Verifica

```sh
node check.mjs
node check-biomes.mjs
node check-tiles.mjs
node check-terraces.mjs
```

Nessuna API meteo: terreno e clima sono simulati nel browser. I settori vengono generati in un worker mentre si esplora. La cartella contiene il modulo WASM per conservare questa anteprima; sorgenti DSP, script di ricompilazione e licenze sono nella cartella ../dsp della repository. Three.js conserva il proprio avviso di licenza nei moduli inclusi.
