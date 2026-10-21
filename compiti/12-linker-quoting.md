# Compito 12: quoting dei path nel backend GCC e ramo Apple del linker (M5+M6)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## 1. Obiettivo

Due finding della code review del compilatore (M5 e M6):
- **M5**: i comandi passati a `system()` da `linker_gcc/linker_gcc.cpp` e
  `codegen_c/codegen_c.cpp` interpolano i path tra doppi apici (o senza apici):
  spazi funzionano, ma `$`, backtick, `"` e `\` rompono la build (iniezione
  shell teorica). Introdurre una funzione unica `shellQuote` e usarla ovunque.
- **M6**: su macOS il linker nativo ld64 non conosce `--start-group/--end-group`
  e `-s` è obsoleto: il percorso "clang su mac" previsto da `linker_gcc.cpp`
  non può linkare. Su Apple (target nativo) omettere il gruppo e `-s`.

### Vincoli assoluti

- Modifiche SOLO a questi quattro file, tutti sotto `src/tools/compiler/`:
  `linker_gcc/linker_gcc.cpp`, `codegen_c/codegen_c.cpp`, `std.h`, `std.cpp`.
- NON toccare `package/package_apk.cpp`: la sua parte di M5 è un altro compito.
- Commenti nel codice in inglese; questo brief e il rapporto in italiano.
- Nessun file di codice sopra le 800 righe. Misure attuali: `linker_gcc.cpp`
  263, `codegen_c.cpp` 761 (vicino al limite: aggiungere solo le righe del
  fix), `std.h` 21, `std.cpp` 2.
- Le modifiche nei rami solo-Apple NON sono verificabili su questa macchina
  (Linux): scriverle «testo alla lettera» come prescritto sotto e rileggere il
  diff riga per riga prima del commit; marcarle così anche nel rapporto.
  Il percorso Linux invece È testabile e non deve regredire.
- Commit dei soli file del fix, messaggio in inglese, riga finale
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Nessun push.
- Strumenti: `blitzpath=/home/rufio72/blitx3d_linux/_release`; il blitzcc di
  questa build vuole `-o` PRIMA del sorgente; la build va fatta con ninja nella
  cartella `build/x86_64-linux-release` (il binario esce direttamente in
  `_release/bin/blitzcc`, è il `RUNTIME_OUTPUT_DIRECTORY` del CMake radice).

## 2. Cosa esiste già (fatti verificati)

Percorsi relativi a `src/tools/compiler/`. Fatti da lettura diretta dei file e
da grep; le misure della sezione 4 sono state eseguite con il blitzcc già
installato.

### 2.1 Chi usa il linker GCC (da grep su tutto `src/tools`)

- Il backend GCC è il default di questa build (`USE_GCC_BACKEND`,
  `CMakeLists.txt:4` e 248-250; `main.cpp:342-344`); il flag `-gcc` lo forza
  (`main.cpp:380-382`).
- `main.cpp:651` chiama `linker.getGccCommand(...)` solo per stamparla come
  `GCC_CMD:` in modalità `-c` (per l'IDE); `main.cpp:656` chiama
  `linker.createExe(debug,rt,target,cFile,out_file)` che esegue la stessa
  stringa con `system()` (`linker_gcc.cpp:256`).
- `Linker_GCC::compileToObject` (riga 117) e `Linker_GCC::linkExecutable`
  (riga 140) NON hanno chiamanti (grep su `src/tools`): codice morto ma
  esportato in `linker_gcc.h`. Vanno sistemati lo stesso (M5 li cita), senza
  rimuoverli.
- `Codegen_C::compileToObject` (`codegen_c.cpp:735`) idem: nessun chiamante,
  ma citato da M5, si corregge senza rimuoverlo.

### 2.2 Punti dove un path entra in una stringa per `system()`

`linker_gcc/linker_gcc.cpp`:

| Riga | Funzione | Cosa | Oggi |
|------|----------|------|------|
| 120  | compileToObject | `cmd << getCompiler(target)` | senza apici (può essere un path NDK) |
| 131  | compileToObject | `" -o \"" << objFile << "\""` | doppi apici |
| 134  | compileToObject | `" \"" << cFile << "\""` | doppi apici |
| 137  | compileToObject | `system(cmd.str().c_str())` | — |
| 154  | linkExecutable | `" -o \"" << exeFile << "\""` | doppi apici |
| 157  | linkExecutable | `" \"" << objFile << "\""` | doppi apici |
| 161  | linkExecutable | `" -L\"" << path << "\""` | doppi apici |
| 175  | linkExecutable | `system(...)` | — |
| 192  | getGccCommand | `" -o \"" << exeFile << "\""` | doppi apici |
| 195  | getGccCommand | `" \"" << cFile << "\""` | doppi apici |
| 199  | getGccCommand | `" -L\"" << path << "\""` | doppi apici |
| 256  | createExe | `system(cmd.c_str())` sulla stringa di getGccCommand | — |

NON toccare: riga 48 (`system("which clang ...")`, stringa costante); le
opzioni `-l<lib>` e `-lbb.<mod>` (nomi interni senza spazi); righe 231-232
(`-framework X` contiene uno spazio voluto: sono due argomenti); il nome del
compilatore restituito da `getCompiler` resta NON quotato come oggi (su
Windows il quoting del primo token di `system()` è infido; il caso NDK con
spazi nel path resta fuori perimetro, vedi domande aperte).

`codegen_c/codegen_c.cpp`:

| Riga | Cosa | Oggi |
|------|------|------|
| 751  | `std::string cmd = compiler + " -c -O2 -o " + outputPath + " " + cFile;` | NESSUN quoting |
| 753  | `system(cmd.c_str())` | — |

`compiler` alle righe 747-750 è la costante `"gcc"`/`"clang"`: non quotarla.

### 2.3 M6: gruppi e `-s`, e come si distingue la piattaforma

- `-Wl,--start-group` a riga 203, `-Wl,--end-group` a riga 222 (getGccCommand),
  emessi incondizionatamente.
- `-s` a riga 150 (linkExecutable, ramo release) e riga 188 (getGccCommand,
  `" -O2 -s"`).
- La piattaforma è distinta a compile time con `#ifdef __APPLE__` (già usato
  alle righe 34, 44, 81 dello stesso file). ATTENZIONE: su host Apple con
  `target.type == "android"` o `"ovr"` il compilatore è il clang dell'NDK
  (riga 39) che usa lld, il quale SUPPORTA i gruppi: il ramo Apple deve quindi
  escludere questi target, non basta l'`#ifdef`.

### 2.4 Dove mettere `shellQuote`

`std.cpp` (2 righe: solo `#include "std.h"`) e `std.h` (21 righe) sono già
compilati in blitzcc (`CMakeLists.txt:139`): casa naturale per l'utility.
Né `linker_gcc.cpp` né `codegen_c.cpp` includono oggi `std.h`: aggiungere
`#include "../std.h"` in entrambi.

## 3. Modifiche da realizzare

### 3.1 `std.h` — dichiarazione (prima della chiusura dell'header guard)

```cpp
// Quote a path argument for a system() command line.
std::string shellQuote( const std::string &s );
```

### 3.2 `std.cpp` — definizione (testo alla lettera)

```cpp
std::string shellQuote( const std::string &s ){
#ifdef _WIN32
    // cmd.exe has no POSIX quoting; keep the current double-quote behaviour.
    return "\""+s+"\"";
#else
    // POSIX sh: single quotes, with embedded ' turned into '\''
    std::string out="'";
    for( size_t i=0;i<s.size();++i ){
        if( s[i]=='\'' ) out+="'\\''";
        else out+=s[i];
    }
    return out+"'";
#endif
}
```

### 3.3 `linker_gcc.cpp` — quoting (M5)

Aggiungere `#include "../std.h"` in testa. Poi sostituire i punti della
tabella 2.2 (righe 131, 134, 154, 157, 161, 192, 195, 199) con la forma:

```cpp
    cmd << " -o " << shellQuote(objFile);   // esempio per la riga 131
    cmd << " " << shellQuote(cFile);        // esempio per la riga 134
    cmd << " -L" << shellQuote(path);       // esempio per la riga 161
```

### 3.4 `linker_gcc.cpp` — ramo Apple (M6, «non verificabile in locale»)

In `getGccCommand`, subito dopo `cmd << getCompiler(target);` (riga 182),
inserire alla lettera:

```cpp
    // ld64 (Apple's native linker) has no --start/--end-group (it resolves
    // archive cycles by itself) and treats -s as obsolete.
#ifdef __APPLE__
    bool ld64 = !(target.type == "android" || target.type == "ovr");
#else
    bool ld64 = false;
#endif
```

Poi: il ramo release diventa `cmd << " -O2"; if (!ld64) cmd << " -s";`
(riga 188); la riga 203 diventa `if (!ld64) cmd << " -Wl,--start-group";`
e la 222 `if (!ld64) cmd << " -Wl,--end-group";`.

In `linkExecutable` (stesso schema, stessa variabile `ld64` calcolata dopo
la riga 144): il ramo release della riga 150 emette `-s` solo se `!ld64`.

Su Linux la stringa generata deve restare IDENTICA a oggi (a parte gli apici
singoli al posto dei doppi): nessun cambio di flag.

### 3.5 `codegen_c.cpp` — quoting (M5)

Aggiungere `#include "../std.h"` e sostituire la riga 751 con:

```cpp
    std::string cmd = compiler + " -c -O2 -o " + shellQuote(outputPath) + " " + shellQuote(cFile);
```

## 4. Verifiche obbligatorie

Prima di toccare il codice, esegui la verifica 0 per confermare lo stato di
partenza; dopo le modifiche esegui tutte le altre. Preparazione comune:

```bash
export blitzpath=/home/rufio72/blitx3d_linux/_release
BB=/home/rufio72/blitx3d_linux/_release/bin/blitzcc
W=$(mktemp -d)
mkdir -p "$W/con spazi/dentro" "$W/con \$dollaro" "$W/con 'apice'"
printf 'Print "ciao"\n' | tee "$W/con spazi/hello.bb" "$W/con \$dollaro/hello.bb" "$W/con 'apice'/hello.bb" > "$W/hello.bb"
```

0. **Stato prima (misurato il 2026-10-21 sul binario attuale)**:
   `$BB -o "$W/con \$dollaro/hello" "$W/con \$dollaro/hello.bb"` fallisce con:
   ```
   cc1: fatal error: .../con /hello.c: File o directory non esistente
   compilation terminated.
   Failed to build executable
   Error: Compilation failed
   ```
   (il `$dollaro` sparisce: espanso dalla shell dentro i doppi apici).
1. **Build**: `ninja -C /home/rufio72/blitx3d_linux/build/x86_64-linux-release blitzcc`
   → exit 0, nessun warning nuovo nei quattro file toccati.
2. **Caso semplice** (il quoting non deve rompere nulla):
   `cd "$W" && $BB -o hello hello.bb` → ultima riga
   `Creating executable "hello"...`, poi `test -x "$W/hello"` → exit 0.
   (Misurato oggi: funziona, binario ~7,1 MB.)
3. **Cartella con spazi** (oggi già funziona, non deve regredire):
   `$BB -o "$W/con spazi/dentro/hello con spazio" "$W/con spazi/hello.bb"`
   → `test -x "$W/con spazi/dentro/hello con spazio"` → exit 0. (Misurato.)
4. **Cartella con dollaro** (oggi fallisce, vedi verifica 0; dopo DEVE passare):
   `$BB -o "$W/con \$dollaro/hello" "$W/con \$dollaro/hello.bb"`
   → `test -x "$W/con \$dollaro/hello"` → exit 0. (Attesa, non misurata.)
5. **Cartella con apice singolo** (oggi funziona; il nuovo quoting a apici
   singoli deve gestire l'apice interno con `'\''`):
   `$BB -o "$W/con 'apice'/hello" "$W/con 'apice'/hello.bb"`
   → `test -x "$W/con 'apice'/hello"` → exit 0. (Misurato oggi: funziona.)
6. **Suite di test** (baseline fornita, lo sviluppatore la misura PRIMA delle
   modifiche e la confronta DOPO):
   `cd /home/rufio72/blitx3d_linux/test && DISPLAY=:1 $BB -r test -o $(mktemp -d)/all_test all.bb`
   → `Pass: 493. Fail: 17.` (baseline dichiarata, non misurata da me).
7. **Nessun file sopra le 800 righe**:
   `wc -l src/tools/compiler/linker_gcc/linker_gcc.cpp src/tools/compiler/codegen_c/codegen_c.cpp src/tools/compiler/std.h src/tools/compiler/std.cpp`
   → tutti sotto 800 (attesi: ~285, ~765, ~24, ~18).
8. **Nessuna modifica fuori ambito**: `git status --short` mostra SOLO i
   quattro file della sezione 3 (prima del commit), vuoto dopo.

## 5. Consegna

- Commit dei soli quattro file, messaggio in inglese, proposta:
  `Compiler: shell-quote paths in GCC backend commands, fix Apple link flags (M5, M6)`
  con riga finale `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  Nessun push.
- Rapporto finale: massimo trenta righe, con l'uscita delle verifiche 1-8
  (per la 0 e la 6 anche la misura "prima"), le deviazioni dal brief e
  l'elenco delle parti marcate «non verificabile in locale».

## 6. Domande aperte

1. «preferenza» — Sede di `shellQuote`: proposta `std.h`/`std.cpp` (già
   compilati in blitzcc, oggi quasi vuoti). Alternativa: header nuovo.
2. «preferenza» — Su Windows `shellQuote` conserva i doppi apici semplici di
   oggi (nessun escape degli apici interni): il backend GCC su Windows passa
   da `cmd.exe`, che non ha quoting POSIX; comportamento invariato rispetto a
   oggi, nessuna regressione possibile.
3. «preferenza» — Su Apple `-s` viene semplicemente omesso (binario non
   strippato su mac, niente `-Wl,-dead_strip`): percorso comunque non
   verificabile in locale, scelta minimale.
4. «preferenza» — Il nome del compilatore (`getCompiler`) resta non quotato:
   il caso «NDK in un path con spazi» resta fuori perimetro (oggi è già rotto
   e quotare il primo token di `system()` su Windows è infido). Se serve, sarà
   un compito a parte.
5. «preferenza» — Le funzioni senza chiamanti (`Linker_GCC::compileToObject`,
   `Linker_GCC::linkExecutable`, `Codegen_C::compileToObject`) si correggono
   ma non si rimuovono: rimozione eventualmente in un compito di pulizia.
