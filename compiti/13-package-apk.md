# Compito 13: package_apk — targetSdk 34, filtro auto-bundle, quoting dei path

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.
Origine: finding M8 e la parte apk di M5 di `CODE_REVIEW_COMPILER.md`
(branch `origin/compiler-code-review`).

## Obiettivo

In `src/tools/compiler/package/package_apk.cpp`: (1) allineare il
`targetSdkVersion` del manifest ad android-34 (oggi è hardcoded "33" mentre
si compila contro android-34/android.jar e la doc dichiara targetSdk 34),
ricavando le versioni da un punto solo; (2) filtrare l'auto-bundle degli
asset, che oggi copia TUTTI i file della cartella sorgente; (3) quotare i
path interpolati nelle stringhe di comando passate a `system()`.

### Vincoli assoluti

- Modifiche solo in `src/tools/compiler/package/` (in pratica solo
  `package_apk.cpp`; `bundle.cpp` è fuori perimetro: il suo `cp` non quotato
  è un fix separato, NON toccarlo).
- Nessun file di codice sopra le 800 righe (`package_apk.cpp` oggi è 188).
- Commenti nel codice in italiano o inglese coerenti col file (il file è in
  inglese: restare in inglese); questo brief e il rapporto in italiano.
- Non toccare il percorso di compilazione normale (non-APK): la suite di
  test deve restare alla baseline.
- Niente push; commit dei soli file del fix.
- Build: `ninja -C build/x86_64-linux-release blitzcc` (cartella build
  esistente sulla macchina locale; NON `build/x86_64-linux-released`).

## Verifica preliminare obbligatoria: quale scenario vale oggi

Il packager APK gira solo nel backend LLVM. **Prima di tutto** stabilisci lo
scenario:

```bash
export blitzpath=/home/rufio72/blitx3d_linux/_release
export PATH=$blitzpath/bin/x86_64-linux-gnu:$PATH
export ANDROID_HOME=/home/rufio72/Android/Sdk
ls /home/rufio72/blitx3d_linux/_release/bin/arm64-android-24/ 2>&1 | head -3
echo 'Graphics 640,480' > $TMP/game.bb; echo 'Flip:WaitKey' >> $TMP/game.bb
blitzcc -target arm64-android-24 -o $TMP/game.apk $TMP/game.bb; echo "exit=$?"
```

- **Scenario A** (il comando produce `$TMP/game.apk`): il toolchain Android
  è presente e funzionante → valgono TUTTE le verifiche, comprese 6 e 7.
- **Scenario B** (toolchain mancante o blitzcc senza supporto LLVM/APK): le
  verifiche sul packaging si riducono a build pulita di blitzcc + controllo
  sintattico standalone (verifica 3) + ispezione statica del diff; dichiara
  nel rapporto quale scenario hai trovato e salta 6 e 7 motivandolo.

Attenzione: se `grep 'USE_LLVM:BOOL' build/x86_64-linux-release/CMakeCache.txt`
dà `OFF`, `ninja ... blitzcc` NON compila `package_apk.cpp` (vedi fatti
CMake sotto): in quel caso la verifica 3 (syntax-only) è l'unico controllo
di compilazione del file modificato ed è obbligatoria.

## Cosa esiste già (fatti verificati)

Tutti i riferimenti sono al commit `704af25` (origin/main).

| File | Righe | Ruolo |
|---|---|---|
| `src/tools/compiler/package/package_apk.cpp` | 188 | unica funzione `createApk(...)`, da modificare |
| `src/tools/compiler/package/package.h` | 16 | firma di `createApk`; include `../bundle.h` e `../target.h` |
| `src/tools/compiler/bundle.h` | 25 | `BundleInfo` con `files` (espliciti) e `sourceDir` |
| `src/tools/compiler/package/bundle.cpp` | 38 | `bundleFiles()`: copia i soli file espliciti `BundleFile` |
| `src/tools/compiler/linker_lld/linker_lld.cpp` | — | chiamante unico: riga 512 `createApk(exeFile,tmpdir,...)`; `tmpdir="tmp/apk"` relativo (riga 43), `androidsdk` da `$ANDROID_HOME` (righe 51-64) |

Fatti puntuali (da lettura diretta del file, commit 704af25):

- `package_apk.cpp:24` — macro `RUN(args)`: `system(std::string(args).c_str())`,
  su errore stampa file:riga e `abort()`.
- `package_apk.cpp:29` — `std::string btversion="34.0.0";` (build-tools).
- `package_apk.cpp:36` — `std::string androidjar=androidsdk+"/platforms/android-34/android.jar";`
  (hardcode n. 2 della versione 34).
- `package_apk.cpp:96-100` — `apiLevel` estratto da `target.version`
  ("android-24" → "24"); usato SOLO per `minSdkVersion` (riga 111): questo è
  corretto e NON va cambiato.
- `package_apk.cpp:112` — `m<<"            android:targetSdkVersion=\"33\" />\n";`
  ← incoerenza M8: il manifest dichiara 33, si compila contro android.jar 34.
- `package_apk.cpp:129-141` — auto-bundle: `RUN("find "+bundle.sourceDir+" -maxdepth 10 -type f ... -exec sh -c '...' _ {} +")`
  con `bundle.sourceDir` e `tmpdir` interpolati NON quotati (M5-apk);
  esclude solo `*.bb`, `*.decls`, nascosti, `*/tmp/*`, `*/.git/*`: tutto il
  resto della cartella sorgente finisce negli asset dell'APK.
- Path non quotati in TUTTE le RUN del file: righe 46-59 (`resdir`, `home`),
  126, 133-140, 157 (`jars`), 164 (`libdir`, `tmpdir`), 169-170 (`resdir`,
  `tmpdir`, `androidjar`, `manifest`), 177 (dopo `chdir`), 180, 183
  (`keystore`), 185, 187 (`out`).
- `bundle.sourceDir` impostato in `main.cpp:535` come path assoluto
  (`realpath` di `.` dopo `chdir` nella cartella del sorgente,
  `main.cpp:514-518`); senza slash finale.
- Direttive esplicite già esistenti nel linguaggio (`toker.cpp:94-96`):
  `BundleIdentifier`, `BundleAppName`, `BundleFile "path"`; il parser
  (`parser.cpp:216-230`) verifica che il file esista e lo mette in
  `bundle.files`, copiato da `bundleFiles()` (chiamata a
  `package_apk.cpp:127`, prima dell'auto-bundle).
- CMake (`src/tools/compiler/CMakeLists.txt`): `package/package_apk.cpp` è
  compilato SOLO sotto `if(LLVM_FOUND)` (righe 199-210);
  `linker_gcc/linker_gcc.cpp` SOLO sotto `if(USE_GCC_BACKEND)` (righe
  249-258), e `USE_LLVM` attivo forza `USE_GCC_BACKEND OFF` (riga 34): i due
  moduli non sono mai compilati nello stesso binario → una `shellQuote` in
  `linker_gcc.cpp` (se il compito 12 l'ha introdotta) NON è linkabile da
  qui: va duplicata localmente come funzione `static`.
- C++17 attivo (`CMakeLists.txt:13` radice); `<filesystem>` già usato nel
  compilatore (`tree/parser.cpp`): usarlo qui è lecito.
- Doc di progetto (CLAUDE.md, sezione Android): minSdk 24, targetSdk 34,
  build-tools 34.0.0 — coerente con l'allineamento a 34.

## Struttura da realizzare (solo `package_apk.cpp`)

### 1. Versioni da un punto solo

In testa al file (dopo gli include), costanti uniche:

```cpp
// Android packaging versions: keep in sync with docs (CLAUDE.md, Android section)
static const char *BUILD_TOOLS_VERSION="34.0.0";
static const char *TARGET_SDK_VERSION="34";
```

- riga 29: `btversion` → usa `BUILD_TOOLS_VERSION`;
- riga 36: `androidjar=androidsdk+"/platforms/android-"+TARGET_SDK_VERSION+"/android.jar";`
- riga 112: `android:targetSdkVersion=\""+std::string(TARGET_SDK_VERSION)+"\"`.
- `minSdkVersion` resta `apiLevel` (da `target.version`): non toccare.

### 2. shellQuote e quoting sistematico

Funzione statica locale (duplicata di proposito: vedi fatto CMake sopra; se
il compito 12 ha già messo una `shellQuote` in `linker_gcc.cpp`, copia
quella implementazione tale e quale per coerenza):

```cpp
// POSIX single-quote escaping: abc'def -> 'abc'\''def'
static std::string shellQuote( const std::string &s ){
	std::string r="'";
	for( char c:s ) r+=( c=='\'' ) ? "'\\''" : std::string( 1,c );
	return r+"'";
}
```

Applicala a OGNI path interpolato nelle stringhe `RUN` elencate nei fatti
(`resdir`, `home`, `tmpdir`, `manifest`, `androidjar`, `libdir+"/"+lib`,
`keystore`, `out`, argomenti di `d8`/`aapt2`/`zipalign`/`apksigner`). Non
quotare le opzioni fisse né `keystorepass` (formato `pass:android`, senza
spazi). La riga 177 (`zip -u unaligned.apk *.dex lib/**/*.so`) usa glob
relativi dopo `chdir(tmpdir)`: lì non c'è nulla da quotare, lascia com'è ma
quota il `chdir` no — `chdir()` è una syscall, non shell: nessuna modifica.

### 3. Auto-bundle filtrato, senza shell

Sostituisci il blocco `find/sh -c` (righe 129-141) con una scansione
`std::filesystem` in C++ (elimina alla radice il problema di quoting di
quel blocco). Regola proposta (vedi domanda aperta 1):

- Se esiste `bundle.sourceDir+"/assets"` (directory): copia ricorsivamente
  SOLO quella in `tmpdir+"/assets"`, senza filtro di estensione.
- Altrimenti: scandisci `bundle.sourceDir` con
  `std::filesystem::recursive_directory_iterator`, profondità max 10
  (`it.depth()`), saltando directory il cui nome inizia per `.` e le
  directory `tmp` (con `it.disable_recursion_pending()`), e copia solo i
  file regolari con estensione (minuscola) in una whitelist di media/dati:
  `png jpg jpeg bmp tga gif wav ogg mp3 mid ttf fnt b3d 3ds x md2 obj mtl
  glb gltf txt json xml csv ini cfg dat`.
- Copia con `std::filesystem::create_directories` +
  `std::filesystem::copy_file(...,copy_options::overwrite_existing)`,
  preservando il path relativo a `sourceDir` (oggi fa lo stesso con
  `${f#sourceDir/}`). Su errore: messaggio su `std::cerr` e `exit(1)`
  (stesso stile del resto del file).
- I file `BundleFile` espliciti restano gestiti da `bundleFiles()` (riga
  127), che gira PRIMA dell'auto-bundle: non toccare quella chiamata.
- Mantieni il messaggio `"Bundling assets from ..."` (riga 131).

Suggerimento di struttura: una `static void autoBundleAssets( const
std::string &sourceDir,const std::string &assetsDir )` sopra `createApk`,
così `createApk` resta leggibile. Stima: il file resta sotto le 280 righe.

## Verifiche obbligatorie

Ambiente per tutte: `export blitzpath=/home/rufio72/blitx3d_linux/_release`,
`export PATH=$blitzpath/bin/x86_64-linux-gnu:$PATH`,
`export ANDROID_HOME=/home/rufio72/Android/Sdk`, `DISPLAY=:1`, `TMP` una
cartella temporanea tua.

1. **Stato PRIMA** (sul codice non modificato) — documenta lo scenario:
   ```bash
   grep -n 'targetSdkVersion' src/tools/compiler/package/package_apk.cpp
   grep -n 'find .*sourceDir' src/tools/compiler/package/package_apk.cpp
   blitzcc -target arm64-android-24 -o $TMP/game.apk $TMP/game.bb; echo exit=$?
   ```
   Attesa: riga 112 con `targetSdkVersion=\"33\"`; riga 133 col `find` non
   quotato; l'esito del terzo comando decide Scenario A o B (vedi sopra).
   In Scenario A, in più: `unzip -l $TMP/game.apk | head -30` e
   `$ANDROID_HOME/build-tools/34.0.0/aapt dump badging $TMP/game.apk | grep -i sdk`
   — attesa (non misurata): `targetSdkVersion:'33'`; conserva l'output come
   confronto per la verifica 7.
2. Build pulita del compilatore:
   `ninja -C build/x86_64-linux-release blitzcc` → exit 0, nessun errore.
3. Controllo sintattico standalone del file modificato (obbligatorio, è
   l'unico che compila `package_apk.cpp` se la build locale non ha LLVM):
   ```bash
   g++ -fsyntax-only -std=c++17 -DBB_TRIPLE='"x86_64-linux-gnu"' src/tools/compiler/package/package_apk.cpp && echo SYNTAX-OK
   ```
   Attesa: `SYNTAX-OK` (misurata oggi sul file originale: passa).
4. Suite di test (il packager non deve toccare il percorso normale):
   ```bash
   cd test && DISPLAY=:1 blitzcc -r test -o $TMP/all_test all.bb
   ```
   Attesa: `Pass: 493. Fail: 17` (baseline; i 17 fail sono ambientali).
5. Nessun file di codice sopra le 800 righe tra quelli toccati:
   `wc -l src/tools/compiler/package/package_apk.cpp` → ≤ 800.
6. **Solo Scenario A** — packaging con spazi nel path (prova M5):
   ```bash
   mkdir -p "$TMP/dir con spazi"; cp $TMP/game.bb "$TMP/dir con spazi/"
   echo x > "$TMP/dir con spazi/nota.txt"; echo y > "$TMP/dir con spazi/scratch.o"
   blitzcc -target arm64-android-24 -o "$TMP/game2.apk" "$TMP/dir con spazi/game.bb"; echo exit=$?
   unzip -l "$TMP/game2.apk" | grep -c 'assets/nota.txt'; unzip -l "$TMP/game2.apk" | grep -c 'scratch.o'
   ```
   Attesa (non misurata): exit=0; `assets/nota.txt` presente (1),
   `scratch.o` assente (0). Prima del fix il comando con spazi fallisce o
   imbarca file spuri.
7. **Solo Scenario A** — targetSdk nel manifest:
   ```bash
   $ANDROID_HOME/build-tools/34.0.0/aapt dump badging $TMP/game.apk | grep -i targetsdk
   ```
   Attesa (non misurata): `targetSdkVersion:'34'`.
8. Ambito: `git status --short` mostra SOLO
   `src/tools/compiler/package/package_apk.cpp` modificato.

## Consegna

- Commit del solo `src/tools/compiler/package/package_apk.cpp`, messaggio
  in inglese, ad esempio: `Compiler: align APK targetSdk to 34, filter
  auto-bundled assets, quote shell paths (M8, M5)`, con riga finale:
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Nessun push.
- Rapporto finale: massimo trenta righe, con lo scenario trovato (A o B),
  l'uscita delle verifiche eseguite, le verifiche saltate e perché, e le
  deviazioni dal brief.

## Domande aperte

1. **Regola di filtro dell'auto-bundle** — «scelta di progetto»: cambia la
   semantica visibile (file prima imbarcati non lo saranno più). Proposta:
   `assets/` se esiste → solo quella; altrimenti whitelist di estensioni
   media/dati (elenco nella sezione 3), con `BundleFile` come via esplicita
   già esistente per i casi fuori lista. Alternativa più rigida: SOLO
   `assets/` e `BundleFile`, nessuna whitelist.
2. **Riscrittura del blocco find in `std::filesystem`** invece del solo
   quoting della stringa `find` — «preferenza»: proposta sì (C++17 già
   attivo e `<filesystem>` già usato in `parser.cpp`; elimina la classe di
   bug invece di tamponarla).
3. **`shellQuote` duplicata come `static` locale** invece che condivisa con
   `linker_gcc` — «preferenza»: proposta duplicazione, perché i due moduli
   non sono mai compilati nello stesso binario (fatto CMake); un header
   comune sarebbe possibile ma allarga il perimetro del compito.
4. **Forma delle costanti versione** — «preferenza»: due costanti `static
   const char *` esplicite e adiacenti (proposta), invece di derivare "34"
   da "34.0.0" a runtime.
