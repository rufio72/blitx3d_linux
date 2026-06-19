# Shadow Mapping — Design & Roadmap

Documento di progetto per l'aggiunta dello shadow mapping al renderer OpenGL
(`src/modules/bb/blitz3d.gl/`). Serve come roadmap condivisa: decisioni, fasi e
criteri di verifica. Le caselle `[ ]` tracciano l'avanzamento.

Stato globale: **🟢 v1 implementata** — shadow map prospettica da point light (torcia), vedi §0

---

## 0. REVISIONE 2026-06-19 — adattamento per la TORCIA (point light) + lighting per-pixel

Da quando questo doc fu scritto, due cose sono cambiate e vanno recepite PRIMA di
seguire le fasi sotto (§1–§10), che restano valide come riferimento ma con queste
correzioni che le semplificano e ne cambiano il target.

### 0.1 Il lighting NON è più per-vertice → la "chirurgia" principale è già fatta
Il vincolo §2 / decisione **D1** (split `ambient`/`direct` nel vertex shader perché
Gouraud) è **OBSOLETO**: ora l'illuminazione è **per-pixel nel fragment** (commit
`9901e68`). Il loop luci accumula `Diffuse`/`Specular` nel fragment con `RS.Ambient`
separato. Quindi il test d'ombra diventa banale:

```glsl
// nel loop luci del fragment, per la luce ombreggiante:
float sh = shadowFactor(...);          // 1 = luce, 0 = ombra
Diffuse  += lightColor * nDotVP * attenuation * sh;
Specular += lightColor * pf     * attenuation * sh;
```

`RS.Ambient` resta fuori dal loop → l'ambiente non va in ombra (corretto). **Niente
split di varying, niente cambi al vertex shader.** Sparisce il rischio #1 (§9).

### 0.2 Il target è una POINT light (torcia), non la direzionale
Le fasi §6 partivano dalla **direzionale** (proiezione ortografica) e rimandavano la
point. Ma la feature richiesta è l'ombra della **torcia del party** = point light a
`dm3d.bb`. Due strade:

| | **A — Shadow map prospettica "in avanti"** (consigliata per v1) | **B — Cube shadow map** (omnidirezionale, v2) |
|---|---|---|
| Idea | UNA depth texture 2D, resa dalla posizione torcia guardando lungo lo sguardo, FOV largo (~110–120°) | 6 facce di depth/distanza dalla torcia (cubemap) |
| Pro | 1 sola passata extra, pipeline "spot" del doc (§4.4/§6 Fase 4), copre ciò che il player guarda (avanti) | corretta in TUTTE le direzioni; riusa l'infra cubemap esistente (`_cube_order`, `SetCubeFace`) |
| Contro | ombre solo nel cono in avanti: superfici illuminate ma di lato/dietro restano senz'ombra (fuori frustum → trattate come illuminate) | 6 passate geometria depth-only + sampling cube + bias per-faccia: più lavoro |
| Costo | ~1× passata depth dei caster | ~6× |

**Raccomandazione:** partire da **A** (vediamo subito le ombre di muri/porte/party
davanti, dove l'occhio sta), poi se serve passare a **B** per le ombre laterali.
In un corridoio il cono in avanti copre quasi tutto il visibile.

### 0.3 Piano concreto rivisto (sostituisce l'ordine di §6 per il caso torcia)
1. **Infra depth target** — FBO con **depth texture** campionabile (oggi c'è solo un
   depth *renderbuffer*, `graphics.gl/canvas.cpp`); `glDrawBuffer(GL_NONE)`.
2. **Programma depth-only** — variante di `default.glsl` con `#define SHADOW_PASS`
   (scrive solo profondità; alpha-test opzionale per i mascherati).
3. **Shadow pass** in `World::render(tween)` **dopo** `bbScene->begin(_lights)`
   (`world.cpp:573`) e prima del loop camera: bind FBO ombra, viewport = risoluzione
   ombra, view/proj **della torcia** (view = inversa della sua TForm; **proj
   prospettica** `setPerspProj` con FOV largo — non ortho), iterare gli STESSI
   `unord_mods`/`ord_mods` opachi col programma depth-only. Salvare `bbLightViewProj`.
4. **Sampling nel fragment** — nuovi uniform `bbShadowMap` (sampler2DShadow) +
   `bbLightViewProj` + indice della luce ombreggiante + flag. Trasformare il
   fragment in clip-space luce, PCF 3×3, e moltiplicare SOLO la luce ombreggiante
   (vedi 0.1). Fuori dal frustum della luce → `sh=1` (illuminato).
5. **Bias & qualità** — depth bias + slope-scaled e/o **front-face cull** nel pass
   (anti acne/peter-panning); PCF per bordi morbidi.
6. **API `.bb`** — `LightShadows light, enable` (+ `ShadowMapSize`), opt-in, default
   off (come §4.7). In `dm3d.bb`: `LightShadows light, True` sulla torcia.

### 0.4 Verifica (come in questa sessione)
- Ombre **off** → output identico a oggi (multiply per `sh=1`).
- Scena di test: un cubo su un pavimento + point light di lato → ombra proiettata che
  si muove con la luce (SaveBuffer, display `:1`, ispezione PNG).
- Poi nel dungeon: una porta/stipite proietta ombra dalla torcia mentre cammini.

### 0.5 Nota depth/proiezione (Q1 aggiornata)
`setPerspProj` mappa z in [-1,1] (`blitz3d.gl.cpp`); per `sampler2DShadow` serve
coerenza nel confronto. Da verificare in Fase 1 visualizzando la depth map.

> Il resto del documento (§1–§10) resta come progettazione di riferimento
> (direzionale/spot/cube), ma il percorso ATTIVO per la torcia è questo §0.

---

## 1. Obiettivo e scope

Aggiungere ombre dinamiche proiettate, opt-in, senza alterare il comportamento
di default né le performance delle scene che non le usano.

- **In scope (v1):** una luce **direzionale** (DISTANT) che proietta ombre, con
  shadow map + PCF, attivabile da `.bb`.
- **In scope (v1.5):** luce **spot** (stessa pipeline, proiezione prospettica).
- **Rimandato:** luci **point** (cube shadow map / dual-paraboloid), cascaded
  shadow maps, soft shadows avanzate (VSM/PCSS).

Motivazione: dalle misure di questa fase, il renderer **non** è il collo di
bottiglia per contenuti realistici (castle demo = ~30 draw call, 2000+ FPS); il
divario verso un look "professionale" sono le **feature**, e lo shadow mapping
è quella a maggior impatto visivo.

---

## 2. Vincolo architetturale chiave (da tenere sempre presente)

**L'illuminazione è per-vertice (Gouraud).** In `default.glsl:166-224` diffuse e
specular sono accumulati nel *vertex shader* e passati come `bbVertex_Color`
interpolato. Un fattore d'ombra per-vertice sarebbe inutilizzabile.

→ **Decisione D1:** il test d'ombra va fatto **per-fragment**. Si separa la
componente *ambient* dalla *diretta* (diffuse+specular) in due varying calcolati
per-vertice; nel fragment: `colore = ambient + ombra * diretta`. Mantiene il
lighting per-vertice (economico) ma rende l'ombra corretta per-fragment.

---

## 3. Architettura target: pipeline a due passate

1. **Shadow pass** — render depth-only dal POV della luce in una depth texture,
   una volta per luce ombreggiante, **prima** del ciclo camera in `World::render`
   (la lista `_lights` è già raccolta lì, `world.cpp:558`).
2. **Main pass** — invariato, ma il fragment shader campiona la shadow map e
   modula il termine diretto.

---

## 4. Componenti (cosa esiste / cosa aggiungere)

### 4.1 Render target a depth texture — *nuovo*
- **Esiste:** FBO con depth **renderbuffer** (`graphics.gl/canvas.cpp:984-994`),
  non campionabile.
- **Aggiungere:** FBO con **depth texture** (`GL_DEPTH_COMPONENT24/32F`,
  `GL_TEXTURE_2D`), `glDrawBuffer(GL_NONE)`, compare mode per PCF hardware
  (`sampler2DShadow`).

### 4.2 Matrici della luce — *riuso*
- **Esiste:** `setViewMatrix`, `setOrthoProj`, `setPerspProj`
  (`blitz3d.gl.cpp:460-527`); la luce ha `TForm` in `lights[i]->matrix`.
- **Aggiungere:** calcolo della `lightViewProj` (view = inversa orientamento
  luce; proj ortografica per direzionale, prospettica per spot) e suo salvataggio
  per il main pass.

### 4.3 Programma depth-only — *nuovo, piccolo*
- **Esiste:** compilazione di `defaultProgram` in `begin()` (`blitz3d.gl.cpp:808`).
- **Aggiungere:** variante di `default.glsl` con `#define SHADOW_PASS` che scrive
  solo profondità (con alpha-test opzionale per superfici mascherate → gli
  alberi-sprite proiettano la sagoma giusta).

### 4.4 Shadow pass in `World::render` — *riuso del draw path*
- Bind FBO shadow, viewport = risoluzione shadow, clear depth, set view/proj
  luce, swap a programma depth-only, **iterare gli stessi modelli opachi** già
  raccolti (`unord_mods`/`ord_mods`) chiamando `bbScene->render(mesh,...)`.
- Usare le trasformazioni **tweened** come il main pass (coerenza `CaptureWorld`).

### 4.5 Modifiche shader — *il cuore*
- **Vertex:** separare `bbVertex_Ambient` e `bbVertex_Direct`; calcolare e passare
  la posizione del fragment in **clip space della luce** (`lightViewProj*worldPos`).
- **Fragment:** PCF (3×3) sulla shadow map → fattore [0..1];
  `colore = ambient + shadow * direct`.
- **Nuovi uniform:** `bbShadowMap` (sampler), `bbLightViewProj` (mat4), flag
  "ombre attive" + indice luce ombreggiante.

### 4.6 Bias e qualità — *standard*
- Depth bias + slope-scaled bias e/o **front-face culling** nello shadow pass
  (contro *shadow acne* e *peter-panning*); PCF per bordi morbidi.

### 4.7 API verso `.bb` — *nuovo comando, opt-in*
- `LightShadows light, enable` (+ eventuale `ShadowMapSize n`), registrato come
  `TrisRendered`/`DrawsRendered` (`commands.decls`, `commands.h`,
  `module.link.cpp`). **Default: disattivato.**

---

## 5. Decisioni di design

- **D1** — Test d'ombra per-fragment, ambient/diretta separate. *(deciso, §2)*
- **D2** — v1 = **una sola luce ombreggiante** (la prima con shadows attive).
  *(deciso; multi-shadow rimandato)*
- **D3** — Opt-in via comando; nessun cambiamento di default. *(deciso)*
- **D4** — Risoluzione shadow map: default **2048²**, configurabile. *(proposto)*
- **D5** — Degradare con grazia se la piattaforma non supporta depth texture /
  `sampler2DShadow` (GLES2): ombre disattivate, nessun crash. *(deciso)*

### Questioni aperte (da risolvere in fase 1)
- **Q1** — Convenzione di depth: `setOrthoProj` mappa z in [0,1], `setPerspProj`
  in [-1,1] (`blitz3d.gl.cpp:481/516`). Il confronto in shadow space deve essere
  coerente con la proiezione usata per la luce. **Da verificare.**
- **Q2** — Fitting della frustum ortografica per la direzionale: v1 con
  box attorno a camera/origine e range dato dall'utente; cascade in futuro.
- **Q3** — Chi proietta / chi riceve: sprite mascherati = caster (alpha-test nel
  depth pass), trasparenti esclusi dai caster. Si lega al batching sprite recente.

---

## 6. Fasi (milestone verificabili)

### Fase 1 — Infrastruttura 🔲
- [ ] FBO + depth texture campionabile
- [ ] Programma depth-only (`SHADOW_PASS`)
- [ ] Render della scena dal POV luce nella depth texture
- [ ] **Verifica:** salvare/visualizzare la depth map (deve mostrare la scena
  vista dalla luce)

### Fase 2 — Direzionale che proietta 🔲
- [ ] Split `ambient`/`direct` nel vertex shader (nessun cambiamento visivo a
  ombre disattivate)
- [ ] Passaggio posizione light-space + sampling + PCF base
- [ ] Comando `LightShadows` (opt-in)
- [ ] **Verifica:** un oggetto su un piano proietta un'ombra corretta

### Fase 3 — Tuning 🔲
- [ ] Bias (depth + slope-scaled), front-face cull
- [ ] PCF 3×3, risoluzione configurabile
- [ ] Fitting frustum direzionale (Q2)
- [ ] **Verifica:** castle demo con ombre pulite (no acne / peter-panning),
  screenshot di confronto

### Fase 4 — Spot light 🔲
- [ ] Proiezione prospettica dal cono (riuso pipeline)
- [ ] **Verifica:** spot light proietta ombra a cono

### Rimandato 🔲
- [ ] Point light (cube shadow map — la pipeline cubemap esiste già:
  `_cube_order`, `SetCubeFace`)
- [ ] Cascaded shadow maps (qualità a distanza per la direzionale)

---

## 7. Piano di validazione

- Screenshot via `SaveBuffer` (flusso già usato in questa sessione), display `:1`.
- Confronto a ombre **disattivate**: l'output deve essere **identico** a oggi
  (garantito dall'opt-in e dallo split ambient/direct neutro).
- `DrawsRendered()` per misurare le draw call extra dello shadow pass.
- Scene di test: oggetto+piano (fase 2), castle demo (fase 3), spot (fase 4).

---

## 8. Costo atteso

- Una passata geometria extra **depth-only** (niente lighting/texture → leggera):
  raddoppia le draw call dei **soli shadow caster** (ordine delle decine sulle
  scene reali misurate).
- Costo dominante: fill della shadow map (risoluzione) + PCF per-fragment, en­trambi
  scalabili. Su scene tipiche, trascurabile (ampio margine FPS misurato).

---

## 9. Rischi

1. **Split per-vertex → per-fragment** del lighting senza alterare l'aspetto
   attuale (la chirurgia shader principale).
2. **Convenzione di depth incoerente** tra ortho/persp (Q1).
3. **Fitting frustum direzionale** (risoluzione sprecata / clipping) — Q2.
4. **Compatibilità GLES** (depth texture / `sampler2DShadow`) — mitigata da D5.
5. **Tuning del bias** — parte artigianale, richiede iterazione visiva.
6. **Caster/receiver per sprite mascherati e trasparenti** — Q3.

---

## 10. Riferimenti al codice

- Lighting per-vertice: `src/modules/bb/blitz3d.gl/default.glsl:166-224`
- Light UBO / setup: `blitz3d.gl.cpp` (begin/lighting, ~:330-410)
- Matrici proj/view: `blitz3d.gl.cpp:460-527`
- Compilazione programma: `blitz3d.gl.cpp:808`
- FBO esistente (depth renderbuffer): `graphics.gl/canvas.cpp:984-994`
- Loop di rendering / raccolta luci: `blitz3d/world.cpp:540-664`
- Sistema luci (tipi DISTANT/POINT/SPOT): `blitz3d/light.h`
- Registrazione comandi (modello): `blitz3d/commands.decls`, `commands.h`,
  `module.link.cpp`
