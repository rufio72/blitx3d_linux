# Compito 14: letterali e costanti intere a 64 bit (finding M7)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## Obiettivo

Nei backend a 64 bit (C/GCC, default su Linux, e LLVM) `bb_int_t` è a 64 bit,
ma il compilatore tiene letterali e costanti intere in `int`: `Local x =
5000000000` diventa 705032704 senza errori. Il valore va portato a `bb_int_t`
lungo tutta la catena (toker → parser → `IntConstNode`/`ConstType` → folding →
backend), con overflow dei letterali definito, senza cambiare nulla nelle build
a 32 bit (dove `bb_int_t` resta `int`).

Vincoli assoluti:
- Modifiche solo in `src/tools/compiler/` e in `test/language/literals.bb`.
  Nessuna modifica al runtime (`src/modules`, `src/runtime`).
- **Nessuna modifica a `src/tools/compiler/codegen_llvm/`** e nessuna modifica
  dentro i blocchi `#ifdef USE_LLVM`: il backend LLVM non si compila in locale.
  Il conteggio delle righe `#if/#ifdef/#ifndef/#endif` dei file toccati resta
  quello della tabella sotto, salvo `parser_expr.cpp` (vedi verifica 8).
- Le build `!BB64` (win32, x86 32 bit) devono restare identiche: lì
  `bb_int_t` è `int` e tutto il codice nuovo deve degenerare nel vecchio.
- Nessun file di codice sopra le 800 righe (`codegen_c.cpp` è a 761: aggiungere
  al massimo 39 righe).
- Commenti nel codice in inglese come il resto del file; rapporto in italiano.
- `blitzcc` vuole `-o` PRIMA del sorgente. `blitzpath=/home/rufio72/blitx3d_linux/_release`.
- In questo runtime `Print` scrive solo nella finestra grafica: nei `.bb` di prova
  si usa `WriteStdout` (va già a capo da solo). Le righe `GL ...` vanno su
  stderr/stdout e si filtrano.
- Commit solo dei file del fix, nessun push.

## Cosa esiste già (fatti verificati)

Tipo: `src/config.h:29-35` → `#ifdef BB64 typedef long long int bb_int_t; #else
typedef int bb_int_t;`. `BB64` è definito globalmente (`CMakeLists.txt:394-395`,
`add_compile_definitions(BB64)`) su x86_64/arm64, quindi vale anche per il
compilatore. Il compilatore vede `bb_int_t` tramite `src/tools/compiler/std.h:4`
(`#include "../../config.h"`), incluso da `codegen.h` → `tree/node.h`.
Il C generato usa sempre `typedef int64_t bb_int_t;` (`codegen_c/codegen_c.cpp:19`).
Il runtime `_bbStrToInt` usa `strtoll` su 64 bit (`src/modules/bb/blitz/basic.cpp:171-181`).

Catena del valore (da grep, righe di `src/tools/compiler/`):

| Punto | File:riga | Oggi |
|---|---|---|
| Toker | `tree/toker.cpp:162-169` | INTCONST = sole cifre decimali (mai segno); BINCONST `%[01]+` (171-174), HEXCONST `$[0-9a-f]+` (176-179) |
| Parse decimale | `tree/parser_expr.cpp:342-345` | `result=d_new IntConstNode( atoi( toker->text() ) );` |
| Parse binario | `tree/parser_expr.cpp:355-360` | accumula in `size_t n` (decl. riga 307: `size_t n,k;`), poi `IntConstNode( n )` tronca a int |
| Parse esadecimale | `tree/parser_expr.cpp:361-366` | idem |
| Nodo | `tree/expr/int_const.h:7,8,10` | `int value; IntConstNode( int n ); int intValue();` |
| Nodo | `tree/expr/int_const.cpp:6,11,20,29,43` | ctor; x86 `TNode( IR_CONST,0,0,value )`; `intValue`; `itoa( value )`; C `g->constantInt( value )` |
| LLVM | `tree/expr/int_const.cpp:16` | `llvm::ConstantInt::get( sem_type->llvmType(...),value,true )` — parametro `uint64_t` + signed: **funziona già con 64 bit, non toccare** |
| Interfaccia | `tree/expr/const.h:9` | `virtual int intValue()=0;` |
| Altre implementazioni | `tree/expr/float_const.h:10`, `.cpp:14-28` | `int intValue()`; ramo non-win32 `return value;` (float→int) |
| | `tree/expr/string_const.h:10`, `.cpp:34-35` | `int intValue(){ return atoi( value ); }` |
| Const | `tree/type.h:138,141` | `int intValue;` e `ConstType( int n )`; overload `ConstType( float n )` riga 142 |
| Const da nodo | `tree/decl/var_decl.cpp:19` | `d_new ConstType( c->intValue() )` |
| Nodo da const | `tree/node.cpp:46` | `d_new IntConstNode( c->intValue )` |
| Const da .decls | `libs.cpp:128-129`, `libs.cpp:376-377` | `int n=atoi(...); defType=d_new ConstType( n );` |
| Global C | `tree/prognode.cpp:350` | `initVal = std::to_string( d->defType->intValue ) + "LL";` (dentro `ProgNode::translate3( Codegen_C *g,...)` da riga 279) |
| Data C | `tree/decl/data_decl.cpp:77` | `g->dataValues.push_back( std::to_string( c->intValue() ) );` (array `bb_int_t bbData[]`, codegen_c.cpp:714) |
| Data x86 | `tree/decl/data_decl.cpp:26` | `g->i_data( c->intValue() )` (int) |
| Folding arit. | `tree/expr/arith_expr.cpp:29-33,40-44` | div-zero check; `+ - * / Mod` in `int` (overflow UB) |
| Folding bit | `tree/expr/bin_expr.cpp:14-19` | `& | ^ << >>`; SHR riga 18: `(unsigned)lc->intValue()>>rc->intValue()` |
| Folding unario | `tree/expr/uni_expr.cpp:17-20` | `+ - Abs Sgn` |
| Folding relaz. | `tree/expr/rel_expr.cpp:48-53` | confronti int (ok con 64 bit, risultato bool) |
| Cast | `tree/expr/cast.cpp:13` | `IntConstNode( c->intValue() )` (ok) |
| Backend C | `codegen_c/codegen_c.h:82`, `codegen_c.cpp:603-605` | `std::string constantInt(int i){ return std::to_string(i) + "LL"; }` |
| Backend C SHR runtime | `tree/expr/bin_expr.cpp:74` | `((bb_int_t)((uint64_t)(l) >> (r)))` |
| Backend LLVM | `codegen_llvm/codegen_llvm.cpp:163-165` | `constantInt( int i )` → `APInt( 64,i )`: usato solo per interi interni piccoli (dim.cpp, stmt_seq.cpp, data_decl.cpp:52, prognode.cpp:212). **Non toccare.** |
| Backend x86 | `codegen.h:33,37` | `TNode::iconst` è `int`; `TNode( int op,TNode*,TNode*,int i )` |

Selezione backend (`main.cpp:339-382`): su Linux 64 bit `usegcc=true`; x86
(`Codegen_x86`, main.cpp:551) solo se né gcc né llvm (build 32 bit, o
`-llvm=off`). Con il campo a `bb_int_t` le build `!BB64` restano a `int`.

Altri fatti:
- `Parser::ex( const std::string& )` riporta un errore di compilazione (`tree/parser.h:48`).
- `itoa(int)` sta in `src/stdutil/stdutil.cpp:156`; non serve allargarla: usare `std::to_string`.
- Nessun `.bb` del repo usa esadecimali di 8+ cifre; `_release/help/commands/2d_examples/Xor.bb:1`
  usa `%11110000111100001111000011110000` (32 cifre, bit alto 1).
- `ReadPixel` restituisce `unsigned` esteso a zero (`graphics/canvas.h:79`,
  `graphics/graphics.cpp:440`): su 64 bit oggi `ReadPixel(..) = $FF000000` è falso.
- Nessun `-Werror` attivo (CMakeLists.txt:384 commentato).
- Test: `test/language/literals.bb` (11 righe, usa `Expect cond, "msg"`), incluso da `test/language.bb:15`.

Conteggio righe `#if*/#endif` attuali (da `grep -cE '^\s*#\s*(if|ifdef|ifndef|endif)'`):
type.h 16, const.h 2, int_const.h 6, int_const.cpp 4, float_const.h 6,
float_const.cpp 6, string_const.h 6, string_const.cpp 4, bin_expr.cpp 4,
arith_expr.cpp 4, uni_expr.cpp 4, parser_expr.cpp 0, libs.cpp 6, prognode.cpp 4,
data_decl.cpp 4, codegen_c.h 2, codegen_c.cpp 2.

## Struttura da realizzare

Principio: il tipo dei valori costanti interi nel compilatore diventa `bb_int_t`
(64 bit con BB64, `int` altrimenti). Così le build 32 bit restano identiche.

1. `tree/expr/const.h`: `#include <type_traits>`; dopo gli include aggiungere
   `typedef std::make_unsigned<bb_int_t>::type bb_uint_t;` (nome libero:
   preferenza); riga 9 → `virtual bb_int_t intValue()=0;`.
2. `tree/expr/int_const.h`: `bb_int_t value; IntConstNode( bb_int_t n ); bb_int_t intValue();`.
3. `tree/expr/int_const.cpp`: ctor e `intValue` a `bb_int_t`; riga 11 →
   `TNode( IR_CONST,0,0,(int)value )` (troncamento esplicito, x86 è 32 bit);
   riga 29 → `return std::to_string( value );`. Riga 16 (LLVM) e riga 43 invariate.
4. `float_const.h/.cpp`, `string_const.h/.cpp`: `intValue()` restituisce
   `bb_int_t`. `StringConstNode::intValue`: con BB64 `(bb_int_t)strtoll( value.c_str(),0,10 )`
   (come il runtime `_bbStrToInt`), altrimenti `atoi( value )` invariato
   (`#ifdef BB64`: +3 righe `#if` in string_const.cpp, ammesse). `FloatConstNode`:
   solo il tipo di ritorno, corpo invariato (il ramo win32 resta com'è).
5. `tree/type.h:138,141`: `bb_int_t intValue;` e `ConstType( bb_int_t n )`.
   Con BB64 una chiamata con `int` diventa ambigua tra `bb_int_t` e `float`:
6. `libs.cpp:128` e `:376`: `bb_int_t n=atoi(...)` (così `ConstType( n )` è esatto).
7. `tree/parser_expr.cpp`: funzione statica `parseIntLiteral`/accumulo con
   controllo, solo con BB64; ramo `#else` = codice attuale riga per riga.
   - Decimale: `errno=0; unsigned long long v=strtoull( t.c_str(),0,10 );`
     se `errno==ERANGE || v>(unsigned long long)INT64_MAX` → `ex( "Integer constant too large" )`;
     altrimenti `IntConstNode( (bb_int_t)v )`. Includere `<cerrno>`, `<cstdint>`.
   - Esadecimale/binario: accumulo in `uint64_t`; prima di ogni shift, se i bit
     che uscirebbero sono non nulli (`n>>60` per hex, `n>>63` per bin) →
     `ex( "Integer constant too large" )`. Fino a 16 cifre hex / 64 bin il valore
     è lo schema di bit a 64 bit (`$FFFFFFFFFFFFFFFF` = -1). Semantica di 8 cifre: vedi domanda 1
     (proposta A: valore puro, `$FFFFFFFF` = 4294967295).
   - `-9223372036854775808` non è scrivibile (come in C: è `-` applicato a un
     letterale troppo grande → errore); si scrive `-9223372036854775807-1`.
8. Folding senza UB, larghezza `bb_int_t` (con `int` riproduce il wrap attuale):
   - `arith_expr.cpp:40-42`: `(bb_int_t)((bb_uint_t)a OP (bb_uint_t)b)`.
   - `arith_expr.cpp:43-44`: se `b==-1`: `/` → `(bb_int_t)(0-(bb_uint_t)a)`, `Mod` → `0`
     (oggi `-2147483648 / -1` fa crashare il compilatore con SIGFPE, misurato).
   - `uni_expr.cpp:18-19`: negazione e Abs con `(bb_int_t)(0-(bb_uint_t)v)`.
   - `bin_expr.cpp:17-19`: conteggio mascherato `s=rc->intValue() & (sizeof(bb_int_t)*8-1)`;
     SHL `(bb_int_t)((bb_uint_t)a<<s)`, SHR `(bb_int_t)((bb_uint_t)a>>s)`, SAR `a>>s`.
9. `codegen_c/codegen_c.h:82`, `codegen_c.cpp:603-605`: `constantInt( int64_t i )`;
   se `i==INT64_MIN` restituire `"(-9223372036854775807LL-1)"`, altrimenti
   `std::to_string(i) + "LL"`.
10. `tree/prognode.cpp:350` → `initVal = g->constantInt( d->defType->intValue );`
    e `tree/decl/data_decl.cpp:77` → `g->dataValues.push_back( g->constantInt( c->intValue() ) );`
    (gestione INT64_MIN in un solo punto; preferenza).
11. x86: nessun'altra modifica. `data_decl.cpp:26`, `vector_var.cpp:31`,
    `vector_decl.cpp:16` ricevono `bb_int_t` e lo convertono implicitamente a
    `int`: con `!BB64` è già `int`, con BB64 x86 non è il percorso usato.
12. `test/language/literals.bb`: aggiungere in fondo
    ```
    big = 5000000000
    Expect big / 1000 = 5000000, "5000000000 is a 64-bit literal"
    Expect 2000000000 + 2000000000 > 0, "constant folding does not wrap at 32 bits"
    Expect Str(-5000000000) = "-5000000000", "negative 64-bit literal"
    Expect $FFFFFFFF > 0, "$FFFFFFFF is 4294967295"
    ```
    (l'ultima solo con proposta A della domanda 1).

## Verifiche obbligatorie

File di prova (crearli in `$TMP=$(mktemp -d)`):

`lit64.bb`:
```
Local x = 5000000000
WriteStdout "a " + x
WriteStdout "b " + (2000000000+2000000000)
WriteStdout "c " + (-5000000000)
WriteStdout "d " + 9223372036854775807
WriteStdout "e " + $FFFFFFFF
WriteStdout "f " + $FF000000
WriteStdout "g " + $100000000
WriteStdout "h " + %11111111111111111111111111111111
WriteStdout "i " + (5000000000 / 2)
Const C = 5000000000
WriteStdout "j " + C
WriteStdout "k " + (-7) + " " + 42 + " " + (7 Mod 3) + " " + (-1 Shr 60) + " " + (-8 Sar 1) + " " + (1 Shl 4)
y = 3000000000 : WriteStdout "l " + (y > 2000000000)
WriteStdout "m " + ("x" + 5000000000)
Local big = 2000000000 : WriteStdout "n " + (big*3)
Global gg = 5000000000 : WriteStdout "o " + gg
Read dv : WriteStdout "p " + dv
WriteStdout "q " + (-9223372036854775807-1)
WriteStdout "r " + Abs(-5000000000) + " " + Sgn(-5000000000)
WriteStdout "s " + Int("5000000000") + " " + ("7" + 0)
Data 5000000000
```
`ovf.bb`: `WriteStdout "o " + 99999999999999999999`
`div.bb`: `WriteStdout "p " + (-2147483648 / -1)`

Comando di prova (per ciascun file F):
`cd $TMP && blitzpath=/home/rufio72/blitx3d_linux/_release /home/rufio72/blitx3d_linux/_release/bin/blitzcc -o $TMP/F $TMP/F.bb; echo cc=$?; DISPLAY=:1 timeout 120 $TMP/F 2>/dev/null | grep -v '^GL\|^$'`

1. **Stato PRIMA** (con il `blitzcc` attuale, prima di toccare il codice).
   Misurato il 2026-10-21 su 704af25:
   `lit64` → `a 705032704` / `b -294967296` / `c -705032704` / `d -1` / `e -1` /
   `f -16777216` / `g 0` / `h -1` / `i 352516352` / `j 705032704` /
   `k -7 42 1 15 -4 16` / `l 0` / `m x705032704` / `n 6000000000` / `o 705032704` /
   `p 705032704` / `q 0` / `r 705032704 -1` / `s 705032704 70`.
   `ovf` → `cc=0`, stampa `o -1`. `div` → `cc=136` (SIGFPE nel compilatore dopo "Generating...").
2. Build: `ninja -C /home/rufio72/blitx3d_linux/build/x86_64-linux-release blitzcc`
   → nessun errore (copiare il binario in `_release/bin` se ninja non lo fa; verificare la data).
3. **DOPO** `lit64` (attesa, non misurata; righe e/f/g/h con proposta A):
   `a 5000000000` / `b 4000000000` / `c -5000000000` / `d 9223372036854775807` /
   `e 4294967295` / `f 4278190080` / `g 4294967296` / `h 4294967295` /
   `i 2500000000` / `j 5000000000` / `k -7 42 1 15 -4 16` / `l 1` /
   `m x5000000000` / `n 6000000000` / `o 5000000000` / `p 5000000000` /
   `q -9223372036854775808` / `r 5000000000 -1` / `s 5000000000 70`.
   La riga `k` e la riga `n` devono restare identiche al PRIMA (nessuna regressione su interi piccoli e negativi).
4. `ovf` → compilazione fallita (`cc` diverso da 0) con messaggio contenente
   `Integer constant too large`; anche `$10000000000000000` (17 cifre) → stesso errore.
5. `div` → `cc=0`, stampa `p 2147483648`.
6. C generato: il backend lascia `$TMP/lit64.c` accanto all'eseguibile (main.cpp:637,
   `cFile = out_file + ".c"`). `grep -n 'bbData\[\]' $TMP/lit64.c` e
   `grep -oE '[-]?[0-9]{6,}LL' $TMP/lit64.c | sort | uniq -c`.
   PRIMA (misurato): `static bb_int_t bbData[] = {1, 705032704};` e letterali
   `-1294967296LL` (1), `2000000000LL` (2), `705032704LL` (2).
   DOPO (attesa): `bbData[] = {1, 5000000000LL}`, compaiono `5000000000LL` e
   `3000000000LL`, nessun `705032704LL`.
7. Suite: `cd /home/rufio72/blitx3d_linux/test && DISPLAY=:1 blitzpath=/home/rufio72/blitx3d_linux/_release /home/rufio72/blitx3d_linux/_release/bin/blitzcc -r test -o $TMP/all_test all.bb && DISPLAY=:1 timeout 300 $TMP/all_test | tail -3`
   → baseline prima: `Pass: 493. Fail: 17`; dopo: `Pass: 497. Fail: 17`
   (attesa: +4 Expect di literals.bb; con proposta B della domanda 1 +3 → 496). Nessuna nuova riga di fallimento.
8. Direttive preprocessore: `cd src/tools/compiler && for f in tree/type.h tree/expr/const.h tree/expr/int_const.h tree/expr/int_const.cpp tree/expr/float_const.h tree/expr/float_const.cpp tree/expr/string_const.h tree/expr/bin_expr.cpp tree/expr/arith_expr.cpp tree/expr/uni_expr.cpp libs.cpp tree/prognode.cpp tree/decl/data_decl.cpp codegen_c/codegen_c.h codegen_c/codegen_c.cpp; do echo "$f $(grep -cE '^\s*#\s*(if|ifdef|ifndef|endif)' $f)"; done`
   → stessi numeri della tabella "Conteggio" sopra; `string_const.cpp` e `parser_expr.cpp`
   possono crescere solo di blocchi `#ifdef BB64/#else/#endif` bilanciati (riportare i numeri).
   `git diff --stat src/tools/compiler/codegen_llvm` → vuoto.
9. `git diff -U0 src/tools/compiler/tree/expr/int_const.cpp | grep -c ConstantInt` → `0` (riga LLVM intatta).
10. File grandi: `find src/tools/compiler -name '*.cpp' -o -name '*.h' | xargs wc -l | awk '$1>800 && $2!="totale" && $2!="total"'`
    → uscita vuota (misurato prima: vuota; `codegen_c.cpp` 761, `parser_expr.cpp` 429).
11. `git status --short` → solo i file elencati in "Struttura da realizzare".

## Consegna

Un commit sul branch corrente, solo i file del fix, messaggio in inglese, es.:
```
Compiler: 64-bit integer literals and constant folding (M7)

<corpo breve>

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```
Nessun push. Rapporto finale in italiano, massimo trenta righe: uscita delle
verifiche 1-11 (le righe DOPO di `lit64` per intero), deviazioni dal brief.

## Domande aperte

1. **Esadecimali/binari di 8 cifre (32 bit) con bit alto** — scelta di progetto.
   A) valore puro a 64 bit: `$FFFFFFFF` = 4294967295, `$FF000000` = 4278190080,
   `%1…1` (32 cifre) = 4294967295. Coerente con `ReadPixel` (unsigned esteso a
   zero) e con un linguaggio a 64 bit; cambia `Xor.bb` dell'help e chi usa `$FFFFFFFF` come -1.
   B) compatibilità: fino a 8 cifre hex / 32 bin si estende il segno da 32 bit
   (oggi `$FFFFFFFF` = -1), oltre si usa il valore a 64 bit.
   Proposta: A (nessun `.bb` del repo usa hex di 8+ cifre).
2. **Letterale decimale oltre INT64_MAX** — preferenza. Proposta: errore
   `Integer constant too large` (oggi atoi dà -1 in silenzio). Alternativa: saturare.
3. **Divisione/Mod costante per -1 nel folding** — preferenza. Proposta: wrap
   (`/` → negazione modulare, `Mod` → 0) invece del crash SIGFPE attuale del compilatore,
   anche nelle build 32 bit.
4. **Nome del tipo senza segno** (`bb_uint_t` in `const.h`) — preferenza. Proposta: `bb_uint_t`
   (nessun altro simbolo con questo nome in `src/`, da grep).
5. **Conteggio degli shift mascherato a 63 (31 in 32 bit)** — preferenza. Proposta: sì,
   come fa l'hardware x86; oggi è UB nel compilatore.
