# Normal / Bump Mapping — Design & Step-by-Step Plan

Documento operativo per aggiungere il **normal (bump) mapping** al renderer OpenGL
(`src/modules/bb/blitz3d.gl/`). È la prosecuzione naturale dello **strato A — luce
per-pixel** (commit `9901e68`), già fatto e verificato. Obiettivo finale: rilievo
vero su muri/pavimento del viewer Dungeon Master, ma l'engine resta generico.

Stato globale: **🔵 Progettazione — nessun codice scritto** (questo doc precede lo sviluppo).

> Convenzione caselle: `[ ]` da fare · `[x]` fatto · `[~]` in corso.
> Vedi anche: `Dungeon_Master/STATO_PORTE_3D.md` (analisi iniziale a 5 strati) e
> `SHADOW_MAPPING.md` (altra feature, indipendente).

---

## 1. Obiettivo e scope

- **In scope (v1):** una **normal map tangent-space** per superficie, attivabile da
  `.bb`, che perturba la normale **per-pixel** così l'illuminazione esistente
  (diffuse + specular, luci dir/point/spot) reagisce al rilievo del materiale.
- **In scope (v1):** generatore **procedurale** di normal map (mattoni/malta) in
  Python, sullo stile di `gen_textures.py`.
- **In scope (v1):** **scena di test** sintetica (quad + cubo) per validare prima di
  toccare il dungeon.
- **Rimandato:** integrazione nel dungeon (`dm3d.bb`) — richiede modifiche a parte
  (vedi §8), da fare DOPO che la feature è validata sulla scena di test.
- **Rimandato:** parallax/relief mapping, height-based displacement, normal map da
  asset esterni di alta qualità.

---

## 2. Punto di partenza (cosa esiste già — verificato sul codice)

| Componente | Stato | Dove |
|---|---|---|
| Luce **per-pixel** (loop luci nel fragment) | ✅ fatto | `default.glsl` main fragment (commit `9901e68`) |
| Normale **eye-space** interpolata nel fragment | ✅ `bbVertex_Normal` | `default.glsl:71,165` |
| Posizione **eye-space** nel fragment | ✅ `bbVertex_ViewPos` | `default.glsl:74,172` |
| Normale **world-space** nel fragment | ✅ `bbVertex_WorldNormal` | `default.glsl:69,138` |
| UV per slot nel fragment | ✅ `bbVertex_TexCoord[i]` | `default.glsl:72,153` |
| `bbNormalMatrix` (cofattore ModelView, object→**eye**) | ✅ uniform | `default.glsl:7`; calcolo `blitz3d.gl.cpp:572-584` |
| Enum **`BLEND_DOT3=4`** | ✅ definito, **non gestito** (cade in REPLACE) | `default.glsl:193`; `Blend()` 260-268 |
| Stato texture per-slot `{blend,sphere_map,flags,cube_map}` | ✅ in UBO | shader `BBTextureState` 31-34; C++ `GLTexState` 255-258 |
| API `.bb` `TextureBlend tex,mode` → `ts.blend` → `us.texs[i].blend` | ✅ | `bbTextureBlend` (blitz3d.cpp:436); upload `blitz3d.gl.cpp:750` |

**Conseguenza chiave:** per marcare una texture come normal map **NON serve toccare
le struct C++ né l'UBO** (rischio layout std140 azzerato): si riusa
`Blend == BLEND_DOT3 (4)`, impostabile da `.bb` con `TextureBlend normalTex, 4`.

---

## 3. Decisione architetturale: TANGENT-SPACE con TBN da derivate (NIENTE tangenti nel vertice)

### 3.1 Perché NON object-space (la scelta iniziale va riveduta)
L'analisi originale (`STATO_PORTE_3D.md`) proponeva object-space "perché i muri sono
allineati agli assi, basta `bbNormalMatrix`". Ma i muri di `dm3d.bb` sono costruiti
con **`CreateCube()`** (`dm3d.bb:138`): un cubo ha **6 facce con 6 normali object-space
diverse** (±X, ±Y, ±Z) che condividono UN solo object space. Una singola normal map
object-space (piatto = blu = +Z) sarebbe corretta **solo sulla faccia +Z** e sbagliata
sulle altre 5. Object-space funzionerebbe solo ricostruendo ogni muro come quad
separato orientato +Z → rebuild geometria invasivo.

### 3.2 Perché tangent-space con TBN per-fragment (metodo di Christian Schüler)
Si ricava la base tangente **nel fragment** dalle derivate schermo di posizione e UV
(`dFdx/dFdy`), senza tangenti nel formato vertice:

```glsl
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv){
  vec3 dp1 = dFdx(p),  dp2 = dFdy(p);
  vec2 du1 = dFdx(uv), du2 = dFdy(uv);
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp*du1.x + dp1perp*du2.x;
  vec3 B = dp2perp*du1.y + dp1perp*du2.y;
  float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
  return mat3(T*invmax, B*invmax, N);
}
// N_eye = normalize( TBN * (texel.rgb*2-1) )   con p=bbVertex_ViewPos, uv=bbVertex_TexCoord[i], N=bbVertex_Normal
```

**Vantaggi (perché è la scelta giusta qui):**
- Funziona sui muri **`CreateCube`** attuali (tutte e 6 le facce ricevono un frame
  corretto) → **zero modifiche alla geometria** del dungeon.
- Funziona anche su quad, pavimenti, soffitti, porte, mesh fuse, qualsiasi orientamento.
- **Nessun cambio al formato vertice** (`GLVertex`) → evita del tutto lo "strato B"
  (tangenti) e i suoi rischi di regressione su TUTTO il 3D.
- Normal map **standard tangent-space** (bluastre) → generazione procedurale facile e
  compatibilità con asset esterni.
- ~8 righe di GLSL autocontenute, attive **solo** quando una texture DOT3 è presente.

**Costi / rischi (mitigazioni in §7):**
- `dFdx/dFdy` per-fragment: trascurabile a queste scene; calcolato solo se c'è DOT3.
- `dFdx/dFdy` su **GLES2** richiede `OES_standard_derivatives` → `#ifdef`/guard
  (desktop GL 3.3 e GLES3 ok). Target primario = desktop, quindi non bloccante.
- Convenzione canale verde (OpenGL +Y vs DirectX −Y): fissiamo **OpenGL (+Y su)** e
  generiamo l'asset coerente.

### 3.3 Object-space resta come alternativa documentata
Se in futuro i muri diventano quad +Z dedicati, l'object-space (1 riga:
`N = normalize(bbNormalMatrix * (texel*2-1))`) è più economico. Non lo adottiamo ora.

---

## 4. Pipeline texture: come una superficie riceve diffuse + normal map

In Blitz3D una entity/brush ha fino a 8 slot texture (`EntityTexture e,tex,frame,index`):
- **slot 0** = diffuse (blend default MULTIPLY/REPLACE).
- **slot 1** = normal map, marcata `TextureBlend normalTex, 4` (DOT3).
- Entrambe usano lo stesso set UV (`bbTexCoord0`): sul normal slot lasciare
  `Flags=0` (default) così campiona `bbTexCoord0` come la diffuse (`default.glsl:148-151`).

Lo shader (dopo le modifiche) **scansiona gli 8 slot**, usa l'eventuale slot DOT3 per
la normale e lo **salta** nel blend del colore (così la normal map non tinge il colore).

> Nota: oggi il default blend di una texture è MULTIPLY (`texture.cpp:42`); va impostato
> esplicitamente DOT3 sul solo normal slot.

---

## 5. STEP DI SVILUPPO (ordine consigliato)

### Step 1 — Shader: funzione TBN da derivate `[x]` (commit `050c1fa`)
File `src/modules/bb/blitz3d.gl/default.glsl`, sezione `#ifdef FRAGMENT`.
- Aggiunta `cotangentFrame(N,p,uv)` (vedi §3.2) prima del `main` fragment. ✅
- `dFdx/dFdy` built-in su core/ES3 (target primario); nessuna estensione aggiunta.

### Step 2 — Shader: ricostruire l'ordine del fragment main `[x]` (commit `050c1fa`)
Oggi: calcola `litColor` con `N=normalize(bbVertex_Normal)` → poi `ProcessTexture(0..7)`.
La normal map va campionata **prima** della luce. Nuovo ordine:
1. `vec3 N = normalize(bbVertex_Normal);`
2. **Pre-pass normal map:** loop `i in [0,TexturesUsed)`: se `RS.Texture[i].Blend==BLEND_DOT3`
   e non è cubemap → `vec3 nt = texture(bbTexture[i], bbVertex_TexCoord[i]).rgb*2.0-1.0;`
   `mat3 TBN = cotangentFrame(N, bbVertex_ViewPos, bbVertex_TexCoord[i]);`
   `N = normalize(TBN * nt);` (prendere il **primo** slot DOT3; break).
3. Loop luci (invariato) usando `N` perturbata.
4. `ProcessTexture(i)`: aggiungere guard **`&& RS.Texture[i].Blend!=BLEND_DOT3`** così il
   normal slot non entra nel blend colore.
- Attenzione: lasciare intatto il ramo cubemap (`default.glsl:319-323`) e
  `SampleCube` (usa `bbVertex_WorldNormal`, non tocchiamo i riflessi).
- Opzionale (tuning): **forza normal map** `float bump=1.0;` con
  `nt.xy *= bump;` prima del TBN (1.0 = pieno, 0 = piatto). Per ora hard-coded.

### Step 3 — Asset: generatore procedurale normal map `[x]`
`Dungeon_Master/play_DungeonMaster/gen_normalmap.py` (nuovo). Scelta: **derivare** la
normal map DALLA diffuse muro (`test.png`) così il rilievo coincide con malta/blocchi
visibili (NON un pattern generico scollegato).
- `test.png` ha **luce bakata** (chiaro in alto): risolto con **high-pass**
  (`height = L − blur(L, 48)`) → tiene solo il dettaglio locale, droppa il gradiente.
- denoise leggero → gradiente (`np.gradient`) → `N=(−dX·s, −dY·s, 1)` normalizzata →
  encode `*0.5+0.5`. Convenzione **OpenGL +Y** (verde negato; verificato: malta incassata).
- `strength≈11` per i muri DM (visibile ma non eccessivo).
- Output `test_n.png`. Validato con luce radente: rilievo netto vs muro piatto di controllo.
- Convenzione repo DM: il tool si committa, l'asset `test_n.png` (1.8MB, rigenerabile)
  va in `.gitignore` come `test.png`.

### Step 4 — Scena di test `.bb` `[ ]`
In `_release/samples/...` (scratch, non committare): una scena con
- un **quad piatto** e un **`CreateCube()`** (per provare il caso multi-faccia),
- una **luce direzionale** + una **point light** (per vedere l'highlight muoversi),
- diffuse in slot 0 + normal map in slot 1 (`TextureBlend nrm,4`),
- toggle bump on/off (due entity affiancate o due render) per confronto,
- `RenderWorld` per N frame → `SaveBuffer` (BMP) → confronto immagini.
Verificare: il rilievo deve **reagire alla direzione della luce** (ruotando la luce,
le ombrette dei solchi si spostano). Senza normal map: superficie piatta.

### Step 5 — Build & verifica `[ ]`
Seguire la ricetta corretta (vedi memoria *Rebuild shader gotchas*):
```bash
rm -f src/modules/bb/blitz3d.gl/default.glsl.h
cmake build/x86_64-linux-release            # rigenera .glsl.h (bin2h al configure)
touch src/modules/bb/blitz3d.gl/blitz3d.gl.cpp
ninja -C build/x86_64-linux-release runtime.opengl.so
strings _release/bin/x86_64-linux-gnu/lib/libbb.blitz3d.gl.a | grep -c cotangentFrame
```
Compilare la scena (`blitzcc`, `blitzpath=_release`), eseguire su `DISPLAY=:1`,
`SaveBuffer`→BMP, convertire con PIL e ispezionare. **Criteri di accettazione** in §9.

---

## 6. File toccati (riepilogo)

| File | Modifica | Tipo |
|---|---|---|
| `src/modules/bb/blitz3d.gl/default.glsl` | TBN + riordino fragment + skip DOT3 nel colore | **engine (committare)** |
| `.../play_DungeonMaster/gen_normal.py` (o esteso) | generatore normal map | tool |
| scena `.bb` di test | validazione | scratch (non committare) |
| `blitz3d.gl.cpp` | **nessuna** (riusa `blend`) | — |

---

## 7. Rischi & edge case (decisi in anticipo)

1. **sRGB vs lineare.** Una normal map NON va trattata come sRGB. Verificare come il
   loader carica le texture (probabile RGBA lineare senza correzione gamma → ok). Se
   in futuro si introduce sRGB sulle diffuse, **escludere** lo slot DOT3.
2. **Mipmap.** Mippare una normal map de-normalizza i texel: nello shader si fa già
   `normalize` dopo il TBN, quindi accettabile. No mipmap sull'asset di test (semplicità).
3. **Filtering.** Linear va bene; evitare NEAREST (scalettatura del rilievo).
4. **GLES2** `dFdx/dFdy`: guard con estensione; target primario desktop, non bloccante.
5. **Double-sided / `FlipMesh`.** Il light loop usa `N` eye-space; se una faccia è
   girata, la normale geometrica è già coerente col culling. La normal map perturba
   attorno a `N` interpolata → nessun trattamento speciale richiesto per v1.
6. **Specular.** Con N per-pixel perturbata, lo specular diventa molto più "vivo": dosare
   `EntityShininess` sui materiali del dungeon (oggi quasi tutto opaco).
7. **Interazione col blend del colore.** Garantire che lo slot DOT3 sia **saltato** nel
   loop colore, altrimenti la superficie si tinge di bluastro (bug tipico).
8. **Performance.** `dFdx/dFdy`+TBN solo quando c'è DOT3; scene DM ~poche draw call →
   irrilevante. Misurabile con `DrawsRendered()/TrisRendered()` se serve.

---

## 8. DOPO la validazione: cosa serve per il dungeon (`dm3d.bb`) — NON ora

### 8.0 Modello d'illuminazione "torcia" (chiarito dall'utente 2026-06-19)
Oggi `dm3d.bb` usa **ambient alto (120) + luce direzionale** → tutto piatto e uniforme.
Il dungeon va portato al modello giusto:
- **ambient quasi a zero** (buio di fondo),
- l'unica vera sorgente è una **TORCIA tenuta dal gruppo** = una **point light**
  posizionata vicino alla camera, che si muove col party (warm color, range corto).

Questo non è solo estetica: una point light ravvicinata e mobile è ciò che fa
**risaltare il normal mapping** (i solchi proiettano micro-ombre che si spostano
mentre cammini). Con la direzionale+ambient attuale il bump si vedrebbe pochissimo.
→ **Torcia e normal mapping vanno insieme.**

### 8.1 ✅ PREREQUISITO RISOLTO: point light (commit `bd76881`, 2026-06-19)
Le point light erano rotte (diffuse 0) e le direzionali illuminavano il lato
sbagliato. **Causa:** `setViewMatrix` nega l'asse Z (Blitz LH→GL RH) → modelview con
**determinante negativo**; la normal matrix usava il cofattore (=det·inv-transpose)
confidando che `normalize` cancellasse il det — ma normalize cancella il modulo, NON
il segno → con det<0 le normali erano **negate** (puntavano dentro). **Fix:** calcolo
del det 3×3 e negazione della normal matrix se <0 (`blitz3d.gl.cpp`).
Verificato: la torcia (point light) illumina con falloff radiale corretto.
⚠️ **Effetto collaterale:** tutte le scene con luce **direzionale** hanno il lato
illuminato invertito (ora è quello corretto) → demo come `castle` possono apparire
diverse e richiedere di girare la direzione della luce.

### 8.2 Modifiche materiale/asset (con tangent-space NON serve rebuild geometria)
- Generare una normal map coerente con la diffuse muro (`test.png`) e con gli stipiti.
- Applicare al brush/entity muro: diffuse slot 0 + normal slot 1 (`TextureBlend,4`).
- Decidere se applicarla anche a pavimento/soffitto (`pavi_3.png`, `soffitto.png`).
- Tuning forza bump + shininess; verificare con nebbia e MSAA attivi.
- Valutare l'impatto su porte (ferro/legno) e ritratti (probabile: lasciarli senza).
Questi punti diventeranno una checklist a parte quando la feature è verde sulla test scene.

---

## 9. Criteri di accettazione (checkpoint)

- [ ] Lo shader compila e finisce nel binario (`strings ... | grep cotangentFrame` > 0).
- [ ] Scene SENZA texture DOT3: **pixel-identiche** al pre-modifica (nessuna regressione;
      confrontare con uno screenshot di controllo, es. la sfera/castle).
- [ ] Quad piatto con normal map + luce direzionale: ruotando la luce, i solchi
      mostrano luci/ombre che **si spostano** (prova che N è perturbata per-pixel).
- [ ] **`CreateCube`** con normal map: il rilievo è corretto su **più facce** (prova che
      il TBN per-fragment funziona dove l'object-space fallirebbe).
- [ ] Lo slot normal map **non tinge** il colore (niente dominante blu).
- [ ] Commit isolato dell'engine (solo `default.glsl`), con messaggio descrittivo.

---

## 10. Ordine esecutivo sintetico

1. Step 1+2 shader (TBN + riordino fragment).  2. Step 3 asset procedurale.
3. Step 4 scena di test.  4. Step 5 build+verifica → checkpoint §9.
5. Commit engine.  6. (separato) §8 integrazione dungeon.
```
shader → asset → test scene → build/verify → commit → [poi] dungeon
```

---

## 11. Parallax-occlusion mapping (POM) — FATTO (commit `d9ebfea`)

Estensione del normal mapping: dà **profondità apparente reale** (i mattoni si
spostano e i solchi si occludono mentre la camera si muove), entro il poligono
piatto — niente geometria in più. È il passo oltre il normal mapping (che finge
solo la luce). Per il rilievo *fisico* della silhouette servirebbe displacement/
tessellation (non fatto).

### 11.1 Sorgente altezza: ALPHA della normal map
La height è impacchettata nel **canale alpha** della normal map DOT3 (`1` = faccia
del mattone, `0` = fondo del solco). `LoadTexture` carica sempre RGBA (la canvas
usa 4 canali) quindi l'alpha si preserva; nessuna texture/slot in più.
`gen_normalmap.py` la genera (param `height_depth`, derivata dalla maschera malta).
**No-op se l'alpha è piatto (1.0)** → le vecchie normal map RGB restano identiche.

### 11.2 Shader (`default.glsl`)
- `parallaxUV(heightTex, uv, viewT, scale)`: steep-parallax march a 24 step +
  occlusion refine. Il sampler height è passato con indice COSTANTE (ES-safe).
- `viewT` = direzione vista (frag→occhio) nello spazio tangente, dal TBN
  per-fragment (`cotangentFrame`, lo stesso del normal mapping).
- In `main`: `ApplyParallax` (unrollato sugli 8 slot, stesse guardie del normal
  map) calcola UN solo shift `gParallax` dal primo slot DOT3; `Sample2D` e il
  fetch della normale lo aggiungono → colore e rilievo si muovono insieme.
- `PARALLAX_SCALE` (const, ~0.16) regola la profondità.

### 11.3 Note / limiti
- L'effetto è **sottile da fermo** (il parallax sposta solo le zone dei solchi) e
  **vivo in movimento**; uno screenshot statico lo sottostima.
- Costo: un ray-march per-fragment sui materiali con height (pochi nel dungeon).
  Possibile ottimizzazione: meno step a vista frontale, programma dedicato.
- Asset DM: `gen_normalmap.py` con `height_depth>0` → `test_n.png` RGBA.
