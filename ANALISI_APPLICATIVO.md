# Analisi approfondita dell'applicativo BlitX3D Linux

**Data analisi:** 11 giugno 2026
**Ambito:** solo analisi (nessuna modifica al codice). Copertura: compilatore `blitzcc` (3 backend), runtime core, moduli grafici/3D, audio, I/O, build system, modifiche non committate nel working tree.

---

## 1. Sintesi esecutiva

Il progetto è in buono stato funzionale (la pipeline backend GCC compila e fa girare i sample end-to-end), ma l'analisi ha evidenziato:

- **~25 bug ad alta confidenza**, alcuni gravi: double-free nel canvas GL, heap overflow nelle `Poke*` dei bank, overread in `Mid$`, crash in `Right$("")`, leak sistematici di stringhe nel C generato, `New` con costruttore rotto sul backend x86, `Restore` con offset dimezzato nel backend C.
- **Rischi di sicurezza concreti** nel parsing di file/rete non fidati: `ReadString` su socket TCP alloca con lunghezza non validata, chunk LIST dei WAV con heap overflow, zlib 1.2.12 con CVE-2022-37434 nota.
- **Backend audio OpenAL gravemente incompleto**: `StopChannel`/`ChannelVolume`/`ChannelPitch` sono no-op, thread in busy-wait al 100% di CPU, leak di canali/stream/file descriptor a ogni `PlaySound`, data race su flag non atomico.
- **Ottimizzazioni rendering ad alto impatto non ancora fatte** (fasi 2+ di OPENGL_OPTIMIZATIONS.md): `glGetError` dopo ogni chiamata GL anche in release, upload completo del VBO per ogni sprite per frame, nessuno state sorting, `glReadPixels` sincrono nel percorso cubemap dinamiche.
- **CI completamente morta** (trigger su branch `master` inesistente, runner GitHub ritirati) e **zero test automatici per OOP e backend GCC**, le due feature principali del fork.
- **Repo da pulire**: ~80 artefatti di build non ignorati (eseguibili ELF e `.c` generati nei sample), submodule sporchi per un bug di idempotenza nei CMakeLists di ode/vorbis, file generati committati.

Le modifiche non committate nel working tree (path resolution case-insensitive, `This` contestuale, completamento backend C, fix `delete[]` in pixmap) sono di **buona qualità e pronte al commit dopo pulizia del repo**, idealmente separate in 4-5 commit logici.

---

## 2. Compilatore `blitzcc` (`src/tools/compiler/`)

### 2.1 Bug probabili (alta confidenza)

| # | Problema | Riferimento |
|---|---|---|
| C1 | **`New ClassName(args)` su backend x86: doppia creazione oggetto + double-free del TNode.** Il nodo `__bbObjNew` è inserito sia come argomento del costruttore sia come figlio di `IR_SEQ`: viene emesso due volte (l'espressione restituisce un secondo oggetto NON costruito) e deallocato due volte dal distruttore di `TNode`. | `tree/expr/new.cpp:50-67`, `codegen.h:38` |
| C2 | **Simbolo costruttore con case sbagliato (x86)**: `"_f"+ident+"_Constructor"` con C maiuscola, ma il toker lowercasa gli identificatori → simbolo non risolto, errore di link su ogni `New` con argomenti. | `tree/expr/new.cpp:61` |
| C3 | **`Super` risolve solo nel genitore diretto**: se il metodo è ereditato dal nonno, errore "Method not found in parent class" (mentre `MethodCallNode::semant` risale correttamente la catena). | `tree/expr/method_call.cpp:164-183` |
| C4 | **`this`/`This` case-sensitive fuori dalle classi**: il token THIS non viene lowercasato → `This = 5` e `Print this` creano due variabili distinte. | `tree/parser.cpp:295-297, 1180-1181` |
| C5 | **`Restore label` rotto nel backend C**: ogni voce DATA occupa 2 elementi (tipo+valore) ma il C generato fa `bbData + offset` invece di `bbData + offset*2` → `Read` dopo `Restore` legge dati corrotti. | `tree/stmt/restore.cpp` (translate3) |
| C6 | **Metodi `Private`/`Protected` senza prototipo nel C generato**: `if(d->kind != DECL_FUNC)` invece di `& DECL_FUNC` salta i metodi con modificatori → dichiarazione implicita, errore con GCC≥14. | `tree/prognode.cpp:367` |
| C7 | **Controllo accessi aggirabile**: `getCurrentClass()` deduce la classe da qualunque parametro chiamato `self`; i check Private sui campi usano il tipo statico dell'espressione, non la classe dichiarante. | `environ.cpp:69-77`, `tree/var/field_var.cpp:14-28` |
| C8 | **Backend C: nessun `deleteVars3`** — parametri stringa e stringhe/oggetti locali mai rilasciati a fine funzione → **leak sistematico a runtime** in ogni programma compilato col backend GCC. | `tree/decl/func_decl.cpp:121-154` vs `tree/node.cpp:183-214` |
| C9 | **Inizializzatori `Local s$ = "x"` nel backend C**: assegnazione raw senza `_bbStrStore`/`_bbObjStore` → leak di stringhe nei loop, refcount oggetti saltati. | `tree/decl/var_decl.cpp` (translate3) |
| C10 | **`Delete expr` valuta l'espressione due volte (x86)** — side effect duplicati. | `tree/stmt/delete.cpp:19-31` |
| C11 | **APK: directory ABI hardcoded** `lib/arm64-v8a` indipendentemente da `target.arch`; `tmpdir` relativo alla dir del progetto utente (`rm -rf tmp/apk` dentro il progetto). | `linker_lld/linker_lld.cpp:43,138-142` |
| C12 | **NX: esito linker ignorato** (`system(...)!=-1` è quasi sempre vero). | `linker_lld.cpp:486` |
| C13 | **`-r`/`-target`/`-sign` rifiutati se `-o` è già stato passato** (condizione copia-incollata dall'handler di `-o`). | `main.cpp:390-404` |
| C14 | **`realpath` può restituire NULL** in `openLibs` → assegnazione di `(char*)NULL` a `std::string`, crash. | `libs.cpp:284` |
| C15 | **Blitz array nel backend LLVM: calcolo stride errato** (`sz=sz*sizes[k]/4`, divisione priva di senso) → indici sbagliati su array multidimensionali. Inoltre ordine degli stride invertito tra backend x86 e C. | `tree/var/vector_var.cpp` (translate2) |
| C16 | **`delete_each.h` rompe la build con USE_LLVM** (dichiarazione `translate3` fuori dall'`#ifdef USE_GCC_BACKEND`). | `tree/stmt/delete_each.h:14` |

### 2.2 Divergenze semantiche tra backend (rischio per la compatibilità)

- **Float→int**: x86 arrotonda (`fistp`), LLVM e C troncano → `Int(2.7)` dà 3 su x86 e 2 altrove. Visibile nei giochi (fisica, indici).
- **Interi 32 vs 64 bit**: backend C usa `int64_t` ma le costanti hex sono parse a 32 bit → `$FFFFFFFF` diventa -1 invece di 4294967295; maschere ARGB e wrap-around divergono da x86.
- **Bounds/null check assenti nel backend C** anche in modalità debug (`-d`): niente `__bbArrayBoundsEx`, `__bbNullObjEx` (x86 li emette).
- **cfunc ignorate dal backend C**: le conversioni Str↔CStr per le runtime function dichiarate `!` non vengono emesse.
- **Userlib non gestite** né da LLVM né dal backend C (simboli esterni inesistenti → errore di link).

### 2.3 Lacune OOP (casi limite)

- **Nessun dispatch virtuale**: risoluzione completamente statica sul tipo dichiarato — un `Child` in variabile `Parent` chiama `Parent_Method` silenziosamente. È l'incoerenza semantica più insidiosa per l'utente.
- **Collisione di mangling**: una `Function animal_speak()` libera può essere risolta come metodo di `animal` con argomenti slittati di una posizione, senza diagnostica.
- **Ordine di dichiarazione**: `Class Child Extends Parent` prima di `Class Parent` fallisce (niente two-pass sui super).
- **`Static Method` dichiarabile ma non invocabile** con sintassi OOP (`ClassName\Method()` non esiste); dentro uno Static, `Self` viene accettato e auto-dichiara una variabile locale int.
- **Costruttori e distruttori non ereditati** (`New Child()` con costruttore solo nel padre → "Constructor not found").
- **`Self` consentito fuori dai metodi** (auto-dichiara una variabile).
- `obj\Method()\field` non supportato (errore esplicito).
- `Insert` confronta i tipi per identità ignorando l'ereditarietà (typo "differnt" nel messaggio).

### 2.4 Debito tecnico e performance del compilatore

- **Lookup lineari O(n)** in `DeclSeq::findDecl` su vettori con centinaia di funzioni runtime → semant ~O(n·F). Una hash map affiancata al vector è il singolo miglioramento più redditizio (~30 righe).
- **Backend C**: concatenazioni massive di `std::string` per valore (O(n²) su espressioni profonde); ~500 righe di header runtime hardcoded nel costruttore di `Codegen_C` da tenere sincronizzate a mano col runtime (fragile by design, confermato da `gcc_backend_todo.txt`); escaping stringhe incompleto (caratteri di controllo, NUL); etichette gosub derivate dall'indirizzo heap (build non riproducibili).
- **Stato globale diffuso** (libs.cpp, contatori statici, toker): nulla è rientrante.
- **Funzioni monolitiche**: `parseStmtSeq` ~415 righe, `main()` ~400, `createExe` ~475.
- **Duplicazione massiccia**: `StructDeclNode`/`ClassDeclNode` quasi identici; la mappa tipo→stringa C reimplementata 6 volte; `MethodCallNode` duplica `CallNode`; due parser di firme identici in libs.cpp.
- **Magic number condivisi col runtime senza header comune** (slot size 4, type-id 5/6, offset BBArray).
- `system()` con stringhe non quotate nel linker (`mkdir -p`, `rm -rf` con path utente), `tmpnam` deprecato, path hardcoded (Windows SDK con versioni esatte, clang 17 NDK, `/lib64/ld-linux-x86-64.so.2`).

---

## 3. Runtime grafico e motore 3D

### 3.1 Bug probabili

| # | Problema | Riferimento |
|---|---|---|
| G1 | **Double-free in `GLCanvas::setPixmap(0)`**: `delete pixmap->bits` seguito da `delete pixmap` il cui distruttore fa di nuovo `delete[] bits` → heap corruption. | `graphics.gl/canvas.cpp:1004-1010` |
| G2 | **`Surface::Vertex::operator<` rotto**: `memcmp(...)==-1` (memcmp restituisce un negativo qualsiasi) + confronto su padding e campi non inizializzati (`bone_bones[1..3]`, `bone_weights`). | `blitz3d/surface.h:25-27` |
| G3 | **`std::map<Vector,Vector>` con ordinamento epsilon-based** in `updateNormals`: viola la strict weak ordering → welding normali non deterministico/UB. | `blitz3d/surface.cpp:61-79`, `geom.h:84-94` |
| G4 | **Out-of-bounds sui tipi di collisione in release**: `_objsByType[1000]`/`_collInfo[1000]` con type validato solo in debug; `bbCollisions` non valida mai. | `blitz3d/world.cpp:31,70-80`, `blitz3d.cpp:297-300` |
| G5 | **`setGamma` scrive sempre sul canale rosso** (copia-incolla: `gamma_red` tre volte). | `graphics.sdl/graphics.sdl.cpp:65-69` |
| G6 | **Risorse GL mai rilasciate**: `GLMesh` non cancella VAO/VBO/IBO (le sprite ricreano periodicamente la mesh condivisa → leak GPU ricorrente); `GLCanvas` non cancella texture/FBO/renderbuffer (`FreeImage`/`FreeTexture` leakano); `GLScene` senza distruttore; texture font mai liberate. | `blitz3d.gl/blitz3d.gl.cpp:95-98`, `graphics.gl/canvas.cpp:163-168` |
| G7 | **Leak `GLLight` per ogni luce** (`freeLight` è vuoto) + UAF latente in `~Light` (chiama `rep->markDirty()` dopo `freeLight`). | `blitz3d/light.cpp:20-23`, `blitz3d.gl.cpp:767` |
| G8 | **Shader: shading "acqua" hardcoded per TUTTE le cubemap** — `SampleCube()` applica sempre Fresnel con colore da `FogColor*0.25` e glint speculari: effetto specifico di cubewater.bb cablato nel percorso generico; una sfera cromata viene resa come acqua. | `blitz3d.gl/default.glsl:460-503` |
| G9 | **`EntityShininess` è un no-op**: `RS.BrushShininess` dichiarato e mai usato, speculare sempre attivo con esponente fisso 100. | `default.glsl:281,418` |
| G10 | **`_cube_order` incoerente** tra codice, commenti inline e CUBEMAP_REFERENCE.txt (tre versioni mutuamente contraddittorie, compensate dallo shader): sistema fragile. | `graphics.gl/canvas.cpp:14-21` |
| G11 | **Divisione per zero** in `Object::endUpdate` con `bbUpdateWorld(0)` → velocity inf/NaN propagata all'audio 3D. | `blitz3d/object.cpp:74` |
| G12 | **Cache pixel collisioni mai invalidata** + `glGetTexImage` assente su GLES. | `graphics.gl/canvas.cpp:537` |
| G13 | **Macro `GL()` senza `do{}while(0)`** — rompe `if/else` e check incondizionati. | `graphics.gl/graphics.gl.h:439` |

### 3.2 Rischi

- **Ring-buffer statici di 64 temporanei** per `Matrix`/`Transform` (`geom.h:258-260,401-403`): zero thread-safety e aliasing silenzioso su catene di trasformazioni profonde (gerarchie di bone).
- **`#undef INFINITY` ridefinito a 1e7**: scene con coordinate >10⁷ rompono il culling.
- **Layout std140 senza `static_assert`**: `UniformState`/`LightState` combaciano col GLSL "per fortuna"; aggiungere un campo disallinea tutto silenziosamente (rilevante perché l'ottimizzazione #8 del piano richiede proprio di aggiungere `Direction` al UBO).
- `dynamic_cast` senza null-check sulle luci; `glBlitFramebuffer` con `GL_DEPTH_BUFFER_BIT` incondizionato (può invalidare il blit su alcuni driver); `GL_BGRA` non valido su GLES (buffer 2D neri su mobile); `exit(1)` su errore shader senza cleanup; copia pitch-ignorante da FreeImage; max 65.535 vertici per surface senza controllo (indici `unsigned short` wrappano).

### 3.3 Stato del piano OPENGL_OPTIMIZATIONS.md (verificato nel codice)

Fatte: #1 uniform location caching, #2 UBO subdata, #3 no re-upload indici (solo desktop), #4 normal matrix su CPU, #5 texture state caching (unbind). **Non fatte:** #6 instancing, #7 state sorting, #8 light direction su CPU, #9 blend caching, #10 texture param caching nel 3D, #11 VAO unbind, #12 binding points, #13-16.

### 3.4 Ottimizzazioni a maggior impatto (non nel piano originale)

1. **Disabilitare `glGetError` per chiamata in release** — ogni chiamata GL fa un loop fino a 10 `glGetError()` che serializza i driver threaded. Il blocco `#ifdef NDEBUG` esiste già, commentato (`graphics.gl.h:434-440`). *Sforzo minimo, guadagno CPU probabilmente il più alto in assoluto.*
2. **Sprite: upload completo del VBO condiviso per OGNI sprite per frame** (`sprite.cpp:97-136` + `GLMesh::unlock` che fa `glBufferData` su tutto il buffer): N sprite = N upload completi. Devastante per giochi particle-heavy.
3. **`GL_STATIC_DRAW` + riallocazione per mesh dinamiche**: skinning CPU, MD2 e terreni ROAM rigenerano e ricaricano i buffer ogni frame con l'hint sbagliato.
4. **`downloadData()` (glReadPixels sincrono) dopo ogni blit** verso canvas non-VIDMEM — nel percorso caldo delle cubemap dinamiche (cubewater, mirror).
5. **`textureId()` ri-uploada la texture dopo ogni blit** anche quando il contenuto GPU è già aggiornato (manca distinzione dirty-CPU/dirty-GPU).
6. `lock/unlock` con `glFinish` per **ogni singolo** `setPixel`/`getPixel`; `glGetIntegerv` sincroni per frame; pick/LOS che rivisitano l'intero scene graph a ogni chiamata; `World::update` itera sempre 1000 vettori di tipi.

---

## 4. Runtime core, I/O, audio

### 4.1 Memory safety (i più gravi del progetto)

| # | Problema | Riferimento |
|---|---|---|
| R1 | **Bank: `Poke*` senza check di ampiezza** — `PokeInt(bank, size-1, v)` scrive 3 byte oltre la fine **anche in debug** (le Peek controllano `offset+3`, le Poke no); `debugBank` non controlla `offset<0`; `CopyBank` con count negativo passa in release a `memmove` con size enorme. **Heap overflow controllabile da codice BASIC.** | `bank/bank.cpp:46,70-73,105-122` |
| R2 | **`bbReadString` legge `int len` da file/socket TCP senza validazione** → `new char[len]` con len negativo (terminate) o enorme (DoS). Vettore di attacco diretto via rete. | `stream/stream.cpp:61-72` |
| R3 | **WAV: chunk LIST con size dal file usato per leggere in un buffer da 4096 byte** → heap overflow da file audio malevolo. | `audio/wav_stream.cpp:65-68` |
| R4 | **`bbMid` avanza il puntatore sbagliato** (`while(n-->0 && p<r) e=...` — testa `p`, avanza `e`): `Mid$(s,1,1000000)` legge oltre il buffer. | `string/string.cpp:71-79` |
| R5 | **`Right$("")` crasha** (`substr(-1)` → `out_of_range` non catturata → terminate); **`Replace(s,"",x)` loop infinito**; `Chr$(-1)` UB. | `string.cpp:27-46,124-128` |
| R6 | **`gosub_stack[512]` senza bounds check**: overflow/underflow scatenabile da Gosub ricorsivo → corruzione del flusso di controllo. | `blitz/basic.cpp:530-539` |
| R7 | **`bbCreateBank(-1)`** → `bad_array_new_length` non catturata; resize senza azzeramento della memoria nuova (info-leak heap via `PeekByte`). | `bank.cpp:10,20-30` |
| R8 | `fullfilename()`: ritorno di `realpath` non controllato → lettura di stack garbage. | `stdutil/stdutil.cpp:332` |

**Nota architetturale**: l'unico catch del runtime è su `bbEx` (`stub/stub.cpp:14`); qualunque eccezione STL (`bad_alloc`, `out_of_range`, `filesystem_error`) → `std::terminate` senza messaggio. Molti dei bug sopra diventano crash secchi per questo motivo.

### 4.2 Audio OpenAL (backend di default su Linux) — gravemente incompleto

- **`stop/setPaused/setPitch/setVolume/setPan` sono no-op** (`audio.openal.cpp:146-157`): `StopChannel`, `ChannelVolume` ecc. non funzionano. Regressione funzionale netta rispetto a FMOD.
- **`~OpenALChannel` chiama `join()` incondizionato** su thread mai avviato → `std::system_error` dal distruttore → terminate (`:39-42`).
- **Busy-wait al 100% di CPU** nel thread di streaming per tutta la durata del suono (`:113-115,134-136`).
- **Data race**: `playbackRunning` bool non atomico condiviso tra thread; `playbackMutex` dichiarato e mai usato (`:34-35`).
- **Leak strutturali**: ogni `PlaySound` accumula un `OpenALChannel` + thread handle (rimossi solo a teardown del driver); `~OpenALSound` non libera lo stream (leak memoria + file descriptor per ogni `LoadSound`).
- `getDuration()` sbagliata di un fattore `channels` per OGG, sempre 0 per WAV; `AL_ORIENTATION` con soli 3 float invece di 6 (OOB read dallo stack); file senza estensione → deref NULL (`:241`); `mp3dec_t` statico globale condiviso tra decoder simultanei.
- Buffer di decode condiviso tra tutti i `Ref` dello stesso `AudioStream`: due canali sullo stesso suono si corrompono l'audio a vicenda (`audio/stream.cpp:17-24`).

### 4.3 Filesystem, socket, portabilità

- **`CreateDir`/`DeleteDir`/`CopyFile`/`RenameFile` non implementati su POSIX** — `RTEX("not implemented")`: comandi BASIC core che terminano il programma su Linux (`filesystem.posix/filesystem.posix.cpp:40-61`).
- `openDir("")` → `back()` su stringa vuota (UB); `bbFilePos`/`bbSeekFile` senza il check `debugFile` presente nelle altre funzioni.
- **UDP: `FIONREAD` fallito → `resize((size_t)-1)`** (DoS); `gethostbyname` bloccante sul thread principale (freeze); logica scope-id `%` con `find()!=npos` sbagliata; `bbHostIP` senza bounds check in release.
- **Endianness**: Read/Write raw little-endian, `_bb_finite` con cast `double*`→`int*` ("Intel order!"), `*(int*)buf` disallineato nel WAV parser. Tutto rotto su big-endian (per ora teorico).
- `ecvt` con buffer statico (non thread-safe, rilevante col thread audio); `#pragma pack(1)` su `struct BBStr : public std::string` (fuori contratto ABI, rischio su ARM); `canonicalpath` usa `#ifdef WINDOWS` mentre il resto del codice usa `WIN32` → ramo Windows latente mai compilato (`stdutil.cpp:385`).
- **Performance**: `_bbObjToHandle`/`FromHandle` su `std::map` (O(log n) per conversione in hot path → `unordered_map`); ogni lettura di asse joystick fa un `idle()`/poll SDL completo; `LSet/RSet` O(n²).

---

## 5. Build system, dipendenze, test, igiene del repo

### 5.1 Problemi

- **CI morta**: trigger su `branches: [master]` ma i branch sono `main` e `change_gcc` → non scatta mai; runner `windows-2019`/`ubuntu-20.04` ritirati da GitHub; job `help` referenzia un target Makefile inesistente (`Makefile:76` — `make all` fallirebbe). La pipeline riflette l'upstream blitz3d-ng, non questo fork.
- **Zero test automatici per OOP e backend GCC**: `grep "Class |Method"` in `test/` non trova nulla; le due feature principali del fork sono coperte solo da 3 sample manuali in `_release/samples/oop/`. `test/suite.sh` non ha alcun percorso che eserciti `codegen_c`/`linker_gcc`.
- **CMake**: `ELSEIF()` vuoto (ramo win32 morto, `CMakeLists.txt:97`); `if($ENV{ARCH})` fragile + doppia logica di detection ARCH in conflitto (`:48-66`, `aarch64` su Linux non riconosciuto); blocco `-Wall` interamente commentato (`:383-387`) — i warning sono di fatto spenti su tutto il progetto.
- **Dipendenze vulnerabili/vecchie**: **zlib 1.2.12 (CVE-2022-37434)**, FreeImage fork 2023 (upstream non mantenuto, storicamente pieno di CVE — è la superficie d'attacco principale, decodifica immagini arbitrarie), SDL 2.26.4 (2023), enet 1.3.17 (2020), minimp3 2021, nlohmann json 3.7.3 (2019, **checkout da 265 MB** per i test data), crossguid 2016.
- **Miscompilazione LTO nota e solo aggirata**: il commento in `CMakeLists.txt:204-219` documenta un double-free di `_bbStrConcat` con LTO globale; lo scoping di LTO ai soli tools è un workaround — il probabile UB (aliasing) nel runtime resta latente. (Coerente con la memoria di progetto: non riabilitare l'IPO globale.)

### 5.2 Debito tecnico

- Generazione di sorgenti **in-tree a configure-time** (bin2h per i `.glsl.h`, `rt.*.cpp`, `module.link.cpp`, `commands.h`, `toolchain.toml` in `_release/`): andrebbe spostata in `add_custom_command` a build-time nel binary dir — è la causa della procedura manuale documentata in CLAUDE.md per gli shader.
- File "AUTOGENERATED. DO NOT EDIT OR COMMIT" **tracciati in git** (commands.h, module.link.cpp, default.glsl.h).
- `string(REPLACE ...)` non idempotente in `deps/ode/CMakeLists.txt:7` e `deps/vorbis/CMakeLists.txt:6` (ogni configure aggiunge un `# ` in più) → è la causa dei submodule perennemente "dirty".
- Default divergenti: `make` → release, cmake diretto → **debug con ASan auto-attivato** se Clang; `-Wl,-s` strippa anche blitzcc/IDE (niente backtrace dagli utenti).
- `BB_SOURCE_ROOT` con path assoluto della macchina incorporato nei binari distribuiti.
- TODO_IMPROVEMENTS.md in larga parte ancora aperto (file stub/obsoleti, VR incompleto, duplicazione graphics.gl/blitz3d.gl, doc moduli al 16,7%); documentazione incoerente tra CHANGES.md, oop_implementations.txt e README (nome del costruttore, ereditarietà dei metodi, access modifiers).

### 5.3 Igiene del repository

- **~80 artefatti untracked non ignorati**: eseguibili ELF dei sample (`benchmark`, `dragon`, `Grass`, `flares`, varianti `_t`/`_test`/`_debug`...), `.c` intermedi del backend GCC accanto a ogni `.bb` (la cleanup nel codegen è commentata di proposito), `benchmark_results.txt`, `_release/bin/` (34 MB). Un `git add -A` distratto committerebbe ~150 MB di binari. Servono pattern `.gitignore` dedicati.
- Sporcizia storica tracciata: `Launcher.exe` (x2), `Thumbs.db`, `Sav7115.TMP`, `high_scores.sav`.
- `ISTRUZIONI_COMPILAZIONE.md` (untracked) riferisce il branch `change_gcc` mentre il lavoro è su `main`.

---

## 6. Review delle modifiche non committate (working tree)

Le modifiche sono **4-5 change-set logici di buona qualità**, tutti orientati a far girare su Linux giochi scritti su Windows col backend GCC:

1. **`bbResolvePath`** (path case-insensitive stile Windows) in stdutil + ~25 entry point runtime. Design corretto; punti aperti: `directory_iterator::operator++` può lanciare un'eccezione non-`bbEx` (→ terminate); `bbCopyFile` risolve solo il sorgente e non la destinazione; `bbCreateDir`/`bbDeleteDir` non coperti; **le texture referenziate dentro i .b3d/.x (via `CachedTexture`) restano scoperte** — il caso più comune di casing rotto; `resolvePathCI` nel parser duplica la logica con semantica più debole (non risolve le directory intermedie negli `Include`).
2. **`This` come keyword contestuale** (alias di Self solo dentro le classi) — fatto con attenzione, edge case minori (`Restore this`, `For this = ...` dentro un metodo).
3. **Completamento backend C**: auto-declaration dei prototipi runtime (risolve un problema reale con GCC 14+), inf/nan in `constantFloat` (funziona ma `INFINITY`/`NAN` di `<math.h>` sarebbero più puliti), `Insert Before/After` implementato.
4. **Toolchain**: Release+strip+LTO scoped, multiarch via `gcc -print-multiarch`, NDK clang **17 hardcoded** (si romperà con NDK r27+ — meglio un glob su `lib/clang/*`), SDK macOS via xcrun.
5. **Fix corretto** `delete[] bits` in `pixmap.cpp:12`.

**Verdetto**: pronto al commit dopo (a) pulizia degli artefatti e aggiornamento `.gitignore`, (b) esclusione dei gitlink dirty dei submodule (o fix dell'idempotenza), (c) decisione su ISTRUZIONI_COMPILAZIONE.md; idealmente in commit separati per filone, documentando in CHANGES.md il nuovo comportamento case-insensitive (che in scrittura cambia la semantica: `WriteFile "Save.dat"` ora sovrascrive un eventuale `save.dat`).

---

## 7. Piano d'intervento consigliato (per priorità)

### Priorità 1 — Correttezza/sicurezza (basso sforzo, alto valore)
1. Bank: check di ampiezza sulle `Poke*`, offset negativi, `CopyBank` con count negativo, `CreateBank` negativo (`bank.cpp`).
2. `bbReadString` con limite di lunghezza; validazione chunk size nel WAV parser.
3. Fix `bbMid`/`bbRight("")`/`bbReplace("")` in `string.cpp`.
4. Double-free in `GLCanvas::setPixmap(0)`; bounds sul `gosub_stack`.
5. Catch-all (`std::exception`) accanto a `bbEx` in `stub.cpp` per trasformare i terminate in runtime error leggibili.
6. Aggiornare zlib a ≥1.2.13 (CVE nota).
7. Implementare `CreateDir`/`DeleteDir`/`CopyFile`/`RenameFile` su POSIX (oggi `std::filesystem` li rende banali).

### Priorità 2 — Funzionalità rotte visibili all'utente
8. Backend OpenAL: implementare stop/volume/pitch/pan, sostituire i busy-wait con attese, `atomic<bool>`, fix del `join()` e dei leak di canali/stream.
9. Backend C: `deleteVars3` (leak sistematici), `Restore offset*2`, store con `_bbStrStore`/`_bbObjStore` negli inizializzatori, prototipi per i metodi Private (`& DECL_FUNC`).
10. OOP: fix `New` su x86 (doppia creazione + case del costruttore), `Super` sulla catena completa, costruttori ereditati, `tolower` su This.
11. `EntityShininess` nello shader; rendere lo shading "acqua" delle cubemap opzionale (flag/FX) invece che hardcoded.
12. Fix `setGamma` (canali verde/blu).

### Priorità 3 — Performance (impatto alto)
13. Spegnere `glGetError` per chiamata in release (blocco già pronto, commentato).
14. Batch dell'upload sprite (un solo upload per frame invece di uno per sprite).
15. `GL_DYNAMIC_DRAW` + `glBufferSubData` per mesh skinned/MD2/terreni.
16. Eliminare il `glReadPixels` per blit nel percorso cubemap dinamiche; separare dirty-CPU da dirty-GPU in `textureId()`.
17. State sorting / cache blend e texture param nel 3D (voci #7-#10 del piano esistente).
18. Hash map per `DeclSeq::findDecl` e per gli handle oggetto.

### Priorità 4 — Infrastruttura
19. Riattivare la CI (trigger su `main`, runner correnti, job che builda il backend GCC e lancia `test/all.bb` + test OOP nuovi).
20. Scrivere test OOP e test del backend C nella suite.
21. `.gitignore` per gli artefatti dei sample; fix idempotenza ode/vorbis; smettere di tracciare i file autogenerati.
22. Riabilitare `-Wall` su `src/`; spostare bin2h e la generazione sorgenti a build-time.
23. Aggiornare FreeImage/SDL/enet/minimp3; valutare shallow checkout per json-hpp (265 MB).
24. Investigare la root cause della miscompilazione LTO di `_bbStrConcat` (probabile UB di aliasing nel runtime) invece di limitarsi al workaround.

---

*Documento generato da analisi statica del codice (5 passate parallele: compilatore, grafica/3D, runtime core/audio/I/O, build system, working tree). Nessuna modifica è stata apportata al codice.*
