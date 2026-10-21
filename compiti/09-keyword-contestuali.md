# Compito 09: keyword OOP contestuali (finding I3)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## 1. Contesto e obiettivo

Il fork aggiunge l'OOP a Blitz3D ma dichiara compatibilità coi sorgenti
classici. Oggi 8 parole nuove (`Class`, `Method`, `Super`, `Extends`, `Static`,
`Private`, `Public`, `Protected`, più i token a due parole `End Class` /
`End Method`) sono riservate ovunque: `Local static = 5`, `Function class()`,
`Global method` non compilano più ("Expecting identifier"). Solo `Self`/`This`
sono già contestuali. Obiettivo: rendere contestuali anche le altre keyword
con lo stesso meccanismo, senza cambiare il comportamento dei sorgenti OOP.

### Vincoli assoluti

- Nessuna modifica fuori da `src/tools/compiler/` (più `CHANGES.md` alla radice).
- Nessun file di codice sopra le 800 righe: `tree/parser.cpp` è già a 1199
  righe, quindi va spezzato PRIMA di aggiungere logica (sezione 3.3, è parte
  del compito, non un'opzione).
- Commenti nel codice in inglese (convenzione dei file toccati); questo brief
  e il rapporto in italiano.
- Commit con messaggio in inglese e riga finale
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Nessun push.
- Non toccare `codegen_*`, `linker_*`, `semant`: il fix è solo lexer/parser.

## 2. Cosa esiste già (fatti verificati, da lettura diretta dei file)

Tutti i percorsi sono relativi a `src/tools/compiler/`. I token sono consumati
SOLO in `tree/parser.cpp` (grep su tutto `src/tools/compiler`); `main.cpp:180`
(`dumpKeys`) stampa solo la lista keyword per l'IDE e non va toccato.

### 2.1 Il meccanismo esistente per Self/This (da replicare)

1. `tree/toker.cpp:42-43`: `Self`/`This` registrati come token SELF/THIS.
2. `tree/toker.cpp:203-207`: il toker emette comunque il token ma ne abbassa
   il testo a minuscolo come per gli IDENT (righe 197-200), così quando il
   parser lo tratta da identificatore il nome combacia.
3. `tree/parser.cpp:137-144` `parseIdent()`: accetta IDENT, THIS e SELF.
4. `tree/parser.cpp:247-252` (statement) e 1148-1152 (espressione): se
   `currentClassName.empty()` fa `goto this_as_ident_stmt` /
   `goto this_as_ident_expr`, cioè ricade nel ramo IDENT. Le label stanno a
   parser.cpp:295 (dentro `case IDENT:` di `parseStmtSeq`) e 1180 (dentro
   `case IDENT:` di `parsePrimary`).
5. `currentClassName` (parser.h:32) è impostato solo in `parseMethodDecl`
   (parser.cpp:874-877): non vuoto = stiamo parsando il corpo di un metodo.

### 2.2 Mappa completa dei punti di consumo delle 8 keyword (parser.cpp)

| Token | Punti di consumo |
|---|---|
| CLASS | 515-518 `parseStmtSeq`, solo `scope==STMTS_PROG`, altrimenti errore |
| ENDCLASS | 107 diagnostica in `exp()`; 835 terminatore in `parseClassDecl` |
| METHOD | 798, 821, 825 in `parseClassDecl` (posizione membro) |
| ENDMETHOD | 108 diagnostica in `exp()`; 878 terminatore in `parseMethodDecl` |
| SUPER | 272-293 `parseStmtSeq` (nota: fa `toker->next()` a 275 PRIMA di controllare `currentClassName` a 276); 1158-1178 `parsePrimary` (stessa struttura) |
| EXTENDS | 791 in `parseClassDecl` (dopo il nome della classe) |
| STATIC | 798, 819-822 in `parseClassDecl` (modificatore, richiede METHOD a 821) |
| PRIVATE | 798-799, 802-804 in `parseClassDecl` |
| PUBLIC | 798-799, 805-807 in `parseClassDecl` |
| PROTECTED | 798-799, 808-811 in `parseClassDecl` |

Altri fatti che contano:

- `tree/toker.cpp:38-49`: registrazione delle keyword in `alphaTokes`.
- `tree/toker.cpp:186-193`: fusione a due parole (`End Class` → ENDCLASS):
  avviene nel toker, su qualunque riga, se ident + singolo spazio + parola
  formano una keyword composta. `End` da solo NON è una keyword: diventa
  IDENT "end" (la funzione runtime che termina il programma, zero argomenti).
- `tree/parser.cpp:499` (`case RESTORE`): controlla `toker->next()==IDENT`
  direttamente, quindi `Restore static` non riconoscerebbe l'etichetta anche
  dopo il fix di `parseIdent` — va adeguato (sezione 3.2, punto f).
- `tree/parser.cpp:65-67`: `enum{STMTS_PROG,STMTS_BLOCK,STMTS_LINE}` a scope
  di file; 69: `isTerm()` statica (usata solo da funzioni che restano in
  parser.cpp: parseStmtSeq 307/395, parseIf 895).
- `tree/parser.h`: dichiara tutti i metodi di parsing come membri privati di
  `Parser`; spezzare il .cpp non richiede cambi di firma.
- `CMakeLists.txt` (di `src/tools/compiler`): `set(TREE_SOURCES ...)` elenca
  `tree/parser.cpp tree/parser.h` e `tree/toker.cpp tree/toker.h` alle righe
  107-108; i file nuovi si aggiungono lì. Nessun altro build file elenca
  parser.cpp (grep su tutto il repo; `oop_implementations.txt` e
  `ANALISI_APPLICATIVO.md` lo citano solo come documentazione, non toccarli).
- Sample OOP con output di riferimento committati in `_release/samples/oop/`:
  `test_super.bb`, `test_inheritance.bb`, `test_multi_inheritance.bb`; ognuno
  scrive un file `*_result.txt` nella cwd e i `*_result.txt` attesi sono già
  nel repo accanto ai sorgenti.

### 2.3 Regole contestuali decise (una per keyword)

- `Method`, `Static`, `Private`, `Public`, `Protected`, `Extends`: keyword
  SOLO dentro `parseClassDecl` (intestazione e posizioni membro del corpo
  classe, che non passa da `parseStmtSeq`). In `parseStmtSeq`, `parsePrimary`
  e `parseIdent` sono SEMPRE identificatori: nessuna ambiguità, perché il
  corpo classe elenca solo membri e i corpi dei metodi sono normali
  `parseStmtSeq`. Vale anche dentro i metodi (`Local static=5` in un Method).
- `Super`: keyword solo con `currentClassName` non vuoto (corpo di un
  metodo), identificatore altrove — identico a Self/This. Dentro i metodi
  resta riservata (come Self/This).
- `Class`: dichiarazione solo a inizio istruzione, `scope==STMTS_PROG` E
  token successivo di tipo identificatore (lookahead); altrimenti
  identificatore. Così `class = 5`, `class(x)`, `class 7` (chiamata comando
  con argomento non-identificatore), `class$`, `class.T` restano codice
  classico valido; `Class Foo [Extends Bar]` resta la dichiarazione. Residuo
  ambiguo irriducibile: la chiamata comando classica `class foo` (funzione
  utente `class` con un solo argomento che è un identificatore, senza
  parentesi) viene letta come dichiarazione — vedi domanda aperta 1.
- `End Class` / `End Method`: restano token riservati. Esistono solo nella
  forma a due parole fuse dal toker; `End` da solo resta IDENT e `Class` /
  `Method` da soli sono coperti sopra. Una riga classica `End class` era già
  invalida (la funzione End non accetta argomenti). Da documentare in
  CHANGES.md — vedi domanda aperta 2.

## 3. Struttura da realizzare

### 3.1 `tree/toker.cpp`

Estendere la normalizzazione a minuscolo (righe 203-207) da `THIS || SELF` a
tutti i token contestuali a parola singola: THIS, SELF, SUPER, CLASS, METHOD,
EXTENDS, STATIC, PRIVATE, PUBLIC, PROTECTED (uno `switch` è più leggibile di
una catena di `||`). NON includere ENDCLASS/ENDMETHOD. Aggiornare il commento.

### 3.2 Parser (modifiche di logica)

a) `tree/parser.h`: aggiungere al `Parser` un helper
   `static bool identToke( int toke );` (true per IDENT, THIS, SELF, SUPER,
   CLASS, METHOD, EXTENDS, STATIC, PRIVATE, PUBLIC, PROTECTED), definito in
   parser.cpp. Spostare qui anche l'enum `STMTS_PROG,STMTS_BLOCK,STMTS_LINE`
   (serve ai .cpp nuovi), come enum privato della classe o nel namespace del
   header; `isTerm` resta statica in parser.cpp.
b) `parseIdent()` (parser.cpp:140): sostituire il controllo con
   `if( !identToke( toker->curr() ) ) exp( "identifier" );`. Questo abilita
   le keyword come nomi di variabili, funzioni, parametri, Type, campi
   (`t\method`), classi, metodi, etichette (`.static`), Goto/Gosub.
c) `parseStmtSeq`: aggiungere `case METHOD: case EXTENDS: case STATIC:
   case PRIVATE: case PUBLIC: case PROTECTED:` subito prima di `case IDENT:`
   (ricadono nella label `this_as_ident_stmt`; il testo è già minuscolo per
   la 3.1, quindi il ramo IDENT funziona invariato).
d) `parseStmtSeq`, `case CLASS:` (515-518): nuova logica
   `if( scope==STMTS_PROG && identToke( toker->lookAhead(1) ) ){ dichiarazione
   come oggi }else goto this_as_ident_stmt;`. Scompare l'errore
   "'Class' can only appear in main program" (vedi domanda aperta 4).
e) `parseStmtSeq`, `case SUPER:` (272-293): spostare il controllo
   `currentClassName.empty()` PRIMA del `toker->next()`; se vuoto
   `goto this_as_ident_stmt;`, altrimenti il corpo attuale invariato.
f) `case RESTORE` (499): usare `identToke( toker->next() )` al posto di
   `==IDENT`, così `Restore static` trova l'etichetta.
g) `parsePrimary`: aggiungere `case CLASS: case METHOD: case EXTENDS:
   case STATIC: case PRIVATE: case PUBLIC: case PROTECTED:` prima di
   `case IDENT:` (label `this_as_ident_expr`; in espressione `Class` è sempre
   identificatore: le dichiarazioni sono solo statement). Per `case SUPER:`
   (1158-1178): se `currentClassName.empty()` → `goto this_as_ident_expr;`
   prima del `toker->next()`; altrimenti corpo invariato.
h) `parseClassDecl` / `parseMethodDecl`: NESSUN cambio di logica (lì il
   contesto OOP è certo); si spostano solo di file (3.3).

### 3.3 Spezzare parser.cpp (obbligatorio, file oggi a 1199 righe)

Creare due file nuovi, metodi di `Parser` invariati nelle firme:

- `tree/parser_expr.cpp` (~430 righe): `parseVar()` e
  `parseVar(ident,tag,allowDot)` (righe attuali 594-631),
  `parseVarOrMethodCall` (633-703), `parseExprSeq` (916-925), `parseExpr`,
  `parseExpr1`..`parseExpr6`, `parseUniExpr`, `parsePrimary` (927-1199).
  Include: `../std.h`, `parser.h`, gli header `expr/*.h` e `var/*.h` oggi in
  testa a parser.cpp (righe 13-31 e 53-56) più quelli che il compilatore
  richiede (es. `stmt/...` non serve qui).
- `tree/parser_decl.cpp` (~215 righe): `parseVarDecl` (705-726),
  `parseArrayDecl` (728-741), `parseFuncDecl` (743-765), `parseStructDecl`
  (767-784), `parseClassDecl` (786-840), `parseMethodDecl` (842-886).
  Include: `../std.h`, `parser.h`, gli header `decl/*.h` e `stmt/return.h`.
- `tree/parser.cpp` (resta ~660 righe): costruttore, `parse`, `ex`/`exp`,
  `resolvePathCI`, `parseIdent`, `identToke`, `parseChar`, `parseStmtSeq`,
  `parseTypeTag`, `parseSigilTag`, `parseIf`. Togliere gli include non più
  usati.
- `CMakeLists.txt` di `src/tools/compiler`: aggiungere
  `tree/parser_expr.cpp tree/parser_decl.cpp` in `TREE_SOURCES` accanto a
  `tree/parser.cpp` (riga ~107). Ninja rilancia cmake da solo quando
  CMakeLists cambia.

### 3.4 `CHANGES.md` (radice del repo)

Aggiungere una nota breve in inglese: le keyword OOP sono contestuali; restano
riservate ovunque solo le sequenze `End Class`/`End Method`; dentro i metodi
restano riservate `Self`/`This`/`Super`; caso limite documentato: a scope
programma, a inizio istruzione, `class <identificatore>` è letto come
dichiarazione di classe (una chiamata comando classica in quella forma esatta
richiede le parentesi: `class(foo)`).

### 3.5 File di test da creare

`test/language/` NON va toccato (cambierebbe i conteggi della suite). Creare i
.bb in una cartella temporanea (es. `/tmp/t09/`), non committarli.

`/tmp/t09/classic.bb` — oggi DEVE fallire, dopo DEVE compilare:

```blitz3d
; keyword OOP usate come identificatori classici
Global method = 1
Global extends = 2
Local static = 5
Local class = 10
Local super = 3
Local private = 1
Local public = 2
Local protected = 4
Local self = 6
Local this = 7
static = static + class
method = method + extends
Print static
Print method
Print super + private + public + protected
Print self + this
Print class(21)
class 7
Local t.T = New T
t\class = 4
t\method = "ok"
Print t\class
Print t\method
Restore static
Read a
Print a
For extends = 1 To 3
	Print "giro " + extends
Next
End
.static
Data 99
Function class(x)
	Print "class(" + x + ")=" + (x * 2)
	Return x * 2
End Function
Type T
	Field class
	Field method$
End Type
```

Uscita attesa (attesa, non misurata — ordine: la Print dentro `class()` esce
prima del valore stampato dal chiamante):
`15, 3, 10, 13, class(21)=42, 42, class(7)=14, 4, ok, 99, giro 1, giro 2, giro 3`
(una voce per riga).

`/tmp/t09/oop_names.bb` — keyword come identificatori DENTRO un metodo:

```blitz3d
Class Counter
	Field n
	Method Inc(amount)
		Local static = amount
		Self\n = Self\n + static
	End Method
	Method Get()
		Return Self\n
	End Method
End Class
Local c.Counter = New Counter
c\Inc(5)
c\Inc(7)
Print c\Get()
```

Uscita attesa: `12` (attesa, non misurata).

## 4. Verifiche obbligatorie

Ambiente: `export blitzpath=/home/rufio72/blitx3d_linux/_release`; compilatore
`_release/bin/blitzcc`; l'opzione `-o` va PRIMA del sorgente; per i programmi
che aprono un display usare `DISPLAY=:1` (su :0 SDL va offscreen).

1. **Misura dello stato PRIMA di ogni modifica** (obbligatoria, i numeri
   sostituiscono le attese dei punti 4-6):
   `ninja -C build/x86_64-linux-release blitzcc` → ok;
   `_release/bin/blitzcc -o /tmp/t09/classic /tmp/t09/classic.bb` → DEVE
   fallire con "Expecting identifier" (attesa, non misurata);
   compilare ed eseguire i tre sample OOP (punto 5) annotando le uscite;
   suite: `cd test && ../_release/bin/blitzcc -r test -o /tmp/t09/all_test
   all.bb && /tmp/t09/all_test` → annotare le righe finali `Pass: N. Fail: M`
   (attesa, non misurata: `Pass: 482. Fail: 17`).
2. Dopo le modifiche: `ninja -C build/x86_64-linux-release blitzcc` →
   compila senza errori.
3. `_release/bin/blitzcc -o /tmp/t09/classic /tmp/t09/classic.bb &&
   /tmp/t09/classic` → compila e stampa l'uscita attesa in 3.5 (attesa, non
   misurata).
4. `_release/bin/blitzcc -o /tmp/t09/oop_names /tmp/t09/oop_names.bb &&
   /tmp/t09/oop_names` → stampa `12` (attesa, non misurata).
5. Per ciascun sample in `_release/samples/oop/` (test_super.bb,
   test_inheritance.bb, test_multi_inheritance.bb): compilare con `-o` in
   /tmp/t09, eseguire da una cartella temporanea vuota e confrontare il
   `*_result.txt` generato con quello committato accanto al sorgente
   (`diff`) → identici, e identici a quanto misurato al punto 1.
6. Suite completa come al punto 1 → stessi identici numeri Pass/Fail della
   misura PRIMA (nessuna regressione, nessun miglioramento atteso).
7. `wc -l src/tools/compiler/tree/*.cpp src/tools/compiler/tree/*.h | sort -n`
   → nessun file sopra le 800 righe.
8. `git status --short` → solo i file previsti: `tree/toker.cpp`,
   `tree/parser.h`, `tree/parser.cpp`, `tree/parser_expr.cpp` (nuovo),
   `tree/parser_decl.cpp` (nuovo), `CMakeLists.txt` del compilatore,
   `CHANGES.md`. Niente in `/tmp` committato.

## 5. Consegna

- Un commit solo, sul branch di lavoro indicato dall'orchestratore, messaggio
  in inglese (es. "Compiler: make OOP keywords contextual (I3)" + 3-5 righe
  sul meccanismo e sullo split di parser.cpp), riga finale
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Nessun push.
- Rapporto finale: massimo trenta righe con l'esito delle 8 verifiche
  (inclusi i numeri misurati al punto 1 e quelli finali) e le deviazioni dal
  brief.

## 6. Domande aperte

1. «scelta di progetto» — Ambiguità irriducibile di `Class`: a scope
   programma, a inizio istruzione, `class foo` (chiamata comando classica di
   una funzione utente `class` con un solo argomento identificatore, senza
   parentesi) è indistinguibile da `Class Foo`. Proposta: preferire la
   dichiarazione di classe e documentare in CHANGES.md che quella chiamata
   richiede le parentesi `class(foo)`; ogni altro uso classico di `class`
   resta valido. Alternativa scartata: lookahead più profondo (ident +
   newline/Extends), che resta comunque ambiguo quando la chiamata chiude la
   riga.
2. «preferenza» — `End Class`/`End Method` restano token riservati (la fusione
   a due parole del toker non si tocca): `End` da solo resta la funzione
   runtime e una riga `End class` era già invalida nel classico. Proposta:
   lasciare così e documentare in CHANGES.md.
3. «preferenza» — Dentro i corpi dei metodi `Super` resta riservata (come già
   Self/This): lì `super` non è usabile come variabile. Come nome di campo
   (`obj\super`) funziona ovunque. Proposta: accettare, è il meccanismo
   Self/This.
4. «preferenza» — Diagnostica degradata: `Class X` dentro una funzione non dà
   più "'Class' can only appear in main program" ma un errore semant di
   funzione inesistente; `Super\...` fuori dai metodi non dà più "Super can
   only be used inside a method". Proposta: accettare (prezzo della
   contestualità).
5. «preferenza» — Nomi dei moduli nuovi: `parser_expr.cpp` e
   `parser_decl.cpp` con la ripartizione in 3.3. Proposta: come scritto.
