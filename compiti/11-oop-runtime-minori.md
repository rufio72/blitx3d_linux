# Compito 11: null-check sui metodi, constructor/destructor Static, costruttore ereditato (M1, M2, M3)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## Obiettivo

Tre correzioni minori all'OOP di `blitzcc` (finding M1, M2, M3 di
`CODE_REVIEW_COMPILER.md`, ramo `origin/compiler-code-review`):
- **M1**: in compilazione debug (`-d`) una chiamata a metodo su un oggetto
  Null o già cancellato deve dare l'errore runtime Blitz «Object does not
  exist» invece di eseguire il metodo con `self` nullo (backend C e LLVM).
  Insieme: `Delete` di un oggetto Null con destructor non deve chiamare il
  destructor (oggi segfault, anche senza `-d`), solo backend C.
- **M2**: `Static Method constructor` / `Static Method destructor` diventano
  un errore di compilazione esplicito nel parser.
- **M3**: `New Figlio(args)` usa il costruttore della classe antenata più
  vicina, risalendo `superType` come già fa il destructor (`stmt/delete.cpp`).

Vincoli assoluti:
- Nessuna modifica fuori da `/home/rufio72/blitx3d_linux`; dentro, solo i file
  elencati in «Struttura da realizzare». Fotografa `git status --short`
  all'inizio e confrontalo alla fine; non toccare file già modificati o non
  tracciati presenti prima.
- Commenti nel codice in italiano; messaggi di errore del compilatore e
  messaggi `Expect` dei test in inglese, come quelli esistenti.
- Nessun file di codice sopra le 800 righe (il più grande toccato,
  `codegen_c/codegen_c.cpp`, ha 761 righe e cresce di 2).
- Codice LLVM: sulla macchina locale `USE_LLVM:BOOL=OFF`, le parti sotto
  `#ifdef USE_LLVM` NON vengono compilate. Le righe nuove lì dentro si copiano
  **alla lettera** da questo brief (vedi «Prudenza sul codice LLVM»).
- Niente installazioni, niente push. `blitzcc` vuole `-o` PRIMA del sorgente
  e la variabile `blitzpath`.

## Cosa esiste già (fatti verificati)

Percorsi relativi a `src/tools/compiler/` salvo indicazione. Fatti letti dai
sorgenti a `704af25` (= `origin/main`).

| File | Righe | Ruolo |
|---|---|---|
| `tree/expr/method_call.cpp` | 309 | `MethodCallNode::semant` r.7-73, `translate` (x86) r.75-100, `translate2` (LLVM) r.102-121 dentro `#ifdef USE_LLVM`, `translate3` (C) r.123-146 dentro `#ifdef USE_GCC_BACKEND`; `SuperMethodCallNode` r.161-309 |
| `tree/var/field_var.cpp` | 126 | `FieldVarNode::translate` (x86) r.33-38 col check debug; `translate2` r.41-47 e `translate3`/`load3`/`store3` r.53-114 SENZA check |
| `tree/expr/new.cpp` | 138 | `NewNode::semant` r.9-53, `translate` (x86) r.55-79, `translate2` r.81-109, `translate3` r.111-138 |
| `tree/expr/new.h` | 34 | `NewNode`: `ctor_args` r.10, `ctor_decl` r.11, `sem_temp` r.12 |
| `tree/stmt/delete.cpp` | 83 | `DeleteNode::semant` r.7-27 (risalita destructor), `translate3` r.68-82 |
| `tree/parser_decl.cpp` | 197 | `Parser::parseClassDecl` r.97-151, `Parser::parseMethodDecl` r.153-197 |
| `codegen_c/codegen_c.cpp` | 761 | intestazione C generata: typedef `BBObj` r.27, extern runtime r.33-88 (`_bbObjFromHandle` r.87) |
| `codegen_llvm/codegen_llvm.h` | — | `bool debug;` r.18, `CallIntrinsic` r.30, `CastToObjPtr` r.31, `llvm::PointerType *voidPtr;` r.46 |

### M1 (null-check)
- x86: `FieldVarNode::translate` r.35-36 (da lettura):
  `if( g->debug ) t=jumpf( t,"__bbNullObjEx" );` sul puntatore e, dopo
  `mem( t )`, di nuovo su `obj->fields`: controlla oggetto Null **e** oggetto
  cancellato. Stesso check in `stmt/insert.cpp:17,19`, `expr/before.cpp:16`,
  `expr/after.cpp:16`. Il backend x86 rifiuta comunque l'OOP
  (`ProgNode::translate`, commit b3c4aa0).
- Backend C e LLVM: **nessun** null-check in debug, né sui field var né sui
  metodi (grep `NullObj` in `tree/`: solo i 5 punti x86 sopra). Per LLVM
  esiste però il modello di check debug con blocchi: `ArrayVarNode::translate2`
  (`tree/var/array_var.cpp:65-81`: `if( g->debug ){ … BasicBlock::Create …
  CreateCondBr … CallIntrinsic( "_bbArrayBoundsEx",void_ty,0 );
  CreateUnreachable(); SetInsertPoint( cont ); }`).
- Runtime: `_bbNullObjEx()` in `src/modules/bb/blitz/basic.cpp:481-483`
  (`RTEX( "Object does not exist" )`), dichiarata in `blitz.h:193` dentro
  `extern "C"`; `nm -D` sui `.so` di `_release/bin/x86_64-linux-gnu/` mostra
  `T _bbNullObjEx` (simbolo C non decorato). Non è dichiarata
  nell'intestazione C generata da `codegen_c.cpp`.
- `_bbObjDelete` (`basic.cpp:309-335`): ritorna subito se `obj` è nullo;
  alla fine mette `obj->fields=0`. Quindi «cancellato» = `fields==NULL`.
- `Codegen_C::debug` è pubblico (`codegen_c.h:23`); con `-d` il C generato
  chiama `bbStart(..., 1)` (misurato).
- `MethodCallNode::translate3` r.134-135 emette l'oggetto senza controlli;
  il C generato per `p\Hello()` è `bb_fm1p_hello(bb__lp);` (misurato).
- `SuperMethodCallNode` passa il proprio `self`: già controllato all'ingresso
  del metodo chiamante, nessun check necessario.
- `DeleteNode::translate3` r.73-77: con destructor emette
  `bb_obj_t tmp = …; f<dtor>(tmp); _bbObjDelete(tmp);` senza controllare `tmp`:
  `Delete` di un Null con destructor che legge un campo dà segfault anche
  senza `-d` (misurato, `rc=139`), mentre in Blitz `Delete Null` è lecito.

### M2 (Static constructor/destructor)
- `parseMethodDecl` r.155 `std::string methodName=parseIdent();`; r.167-171
  aggiunge il parametro `self` solo se `!isStatic`. Gli identificatori sono
  minuscoli dal toker (`new.cpp:70-71`; i messaggi mostrano `'m3child'`).
- `NewNode::semant` r.34-41 e `translate3` r.127-131 passano sempre l'oggetto
  come primo argomento e saltano il primo parametro; `DeleteNode::translate3`
  r.76 idem. Misurato oggi: `Static Method constructor(a)` + `New M2S(7)` →
  `"m2.bb":7:1:7:1:Too many parameters`; `Static Method destructor()` +
  `Delete o` → errore gcc `too many arguments to function
  'bb_fm2d_destructor'`, `Failed to build executable`.
- `Parser::ex` (`tree/parser.cpp:68-70`) lancia `Ex( s,toker->pos(),incfile )`.

### M3 (costruttore ereditato)
- `NewNode::semant` r.15-25: cerca solo `st->ident + "_constructor"`; con
  argomenti e senza costruttore proprio → `Constructor not found in class '…'`.
- `DeleteNode::semant` r.12-26 (commit 8426c10): ciclo
  `for( StructType *c = st; c; c = c->superType )` su `c->ident +
  "_destructor"`, salva `dtor_decl` e `dtor_ident`.
- Layout: `ClassDeclNode::semant` (`tree/decl/class_decl.cpp:31-56`) copia i
  campi della madre nel figlio agli **stessi indici** (`d->offset=k*4`), i
  campi propri dopo. Il backend C accede a `((BBObj*)p)->fields[offset/4]`
  (`field_var.cpp:53-114`), quindi `Madre_constructor(oggetto_figlio)` scrive
  i campi giusti: è lo stesso meccanismo dei metodi ereditati
  (`MethodCallNode::semant` r.16-30 risale la catena e chiama
  `Madre_Metodo(figlio)`). Il prototipo C del costruttore ha primo parametro
  `bb_obj_t` (misurato nel C generato).
- `NewNode::translate2` r.92 usa già `ctor_decl->name` (nome del Decl trovato):
  nessuna modifica LLVM per M3. `translate` r.72 e `translate3` r.126
  ricostruiscono il simbolo da `ident` (classe del `New`): vanno cambiati.
- Misurato oggi col costruttore proprio: `New OwnB(4)` chiama il costruttore;
  `New OwnZ()` chiama il costruttore senza parametri; `New OwnZ` (senza
  parentesi) non lo chiama; `New OwnE()` con `constructor(v)` →
  `Not enough parameters`.
- Nessun test della suite usa constructor/destructor (grep in `test/`).

### Build e baseline
- Opzione debug di `blitzcc`: `-d` (`main.cpp:125` «debug compile», r.368).
- Baseline suite (compilatore di `704af25`): `Pass: 493. Fail: 17` (17
  ambientali, preesistenti).
- Conteggi preprocessore oggi (`grep -c '^#if'` / `grep -c '^#endif'`):
  `method_call.cpp 4 4`, `new.cpp 2 2`, `new.h 3 3`, `delete.cpp 2 2`,
  `parser_decl.cpp 0 0`, `codegen_c.cpp 1 1`.

## Prudenza sul codice LLVM

L'unico hunk sotto `#ifdef USE_LLVM` è in `MethodCallNode::translate2`
(punto 2). Copialo carattere per carattere, tab compresi. Usa solo membri già
usati altrove nel codice compilato in passato (`g->debug`, `g->voidPtr`,
`g->CastToObjPtr`, `g->CallIntrinsic`, `g->builder`, `g->context`), più
`IRBuilder::CreateIsNull( llvm::Value* )`, API standard di LLVM 19 non
ancora usata nel progetto. Niente include nuovi. Dopo: verifica 9 (diff) e
verifica 10 (conteggi `#if`/`#endif` invariati).

## Struttura da realizzare

1. **`codegen_c/codegen_c.cpp`** (M1): dopo la riga 87
   `    header << "extern bb_obj_t _bbObjFromHandle(bb_int_t handle, BBObjType *type);\n";`
   aggiungere:
   ```cpp
       header << "extern void _bbNullObjEx(void);\n";
       header << "static inline bb_obj_t _bbNullChk(bb_obj_t o){ if( !o || !((BBObj*)o)->fields ) _bbNullObjEx(); return o; }\n";
   ```
2. **`tree/expr/method_call.cpp`** (M1):
   - `translate2` (LLVM): sostituire le righe 111-112
     ```cpp
     	// First argument is the object (self)
     	args.push_back( obj->translate2( g ) );
     ```
     con:
     ```cpp
     	// Primo argomento: l'oggetto (self). In debug un oggetto Null o già
     	// cancellato (fields nullo) dà "Object does not exist" prima della chiamata
     	llvm::Value *self_obj = obj->translate2( g );
     	if( g->debug ){
     		auto cur_func=g->builder->GetInsertBlock()->getParent();
     		auto void_ty=llvm::Type::getVoidTy( *g->context );

     		auto null_ex=llvm::BasicBlock::Create( *g->context,"nullobj_ex",cur_func );
     		auto null_chk=llvm::BasicBlock::Create( *g->context,"nullobj_fields",cur_func );
     		auto null_cont=llvm::BasicBlock::Create( *g->context,"nullobj_cont",cur_func );

     		g->builder->CreateCondBr( g->builder->CreateIsNull( self_obj ),null_ex,null_chk );

     		g->builder->SetInsertPoint( null_chk );
     		auto fields=g->builder->CreateLoad( g->voidPtr,g->CastToObjPtr( self_obj ) );
     		g->builder->CreateCondBr( g->builder->CreateIsNull( fields ),null_ex,null_cont );

     		g->builder->SetInsertPoint( null_ex );
     		g->CallIntrinsic( "_bbNullObjEx",void_ty,0 );
     		g->builder->CreateUnreachable();

     		g->builder->SetInsertPoint( null_cont );
     	}
     	args.push_back( self_obj );
     ```
     (`fields` è il primo membro di `BBObj`, offset 0: il load dal puntatore
     all'oggetto legge `obj->fields`.)
   - `translate3` (C): sostituire le righe 134-135
     ```cpp
     	// First argument is the object (self)
     	result += obj->translate3( g );
     ```
     con:
     ```cpp
     	// Primo argomento: l'oggetto (self). In debug un oggetto Null o già
     	// cancellato dà "Object does not exist" prima della chiamata
     	if( g->debug ) result += "_bbNullChk(" + obj->translate3( g ) + ")";
     	else result += obj->translate3( g );
     ```
   - Nessun'altra modifica al file (`translate` x86, `SuperMethodCallNode`
     invariati).
3. **`tree/stmt/delete.cpp`** (M1, Delete Null; domanda 3): in `translate3`
   sostituire la riga 76
   `		g->emitLine( g->toCSafeName( "f" + dtor_ident ) + "(" + tmp + ");" );`
   con:
   ```cpp
   		// Delete di un Null (lecito in Blitz) o di un oggetto già cancellato
   		// non chiama il destructor; _bbObjDelete li ignora già da sé
   		g->emitLine( "if( " + tmp + " && ((BBObj*)" + tmp + ")->fields ) " + g->toCSafeName( "f" + dtor_ident ) + "(" + tmp + ");" );
   ```
   Aggiornare anche il commento r.71-72 se serve, senza cambiarne il senso.
   `translate2` (LLVM) invariato (vedi domanda 3).
4. **`tree/parser_decl.cpp`** (M2): in `parseMethodDecl`, subito dopo la riga
   156 `	std::string tag=parseTypeTag();` aggiungere:
   ```cpp

   	// constructor e destructor ricevono sempre l'oggetto come primo argomento
   	// (NewNode, DeleteNode): non possono essere Static
   	if( isStatic && (methodName=="constructor" || methodName=="destructor") ){
   		ex( methodName+" cannot be a Static Method" );
   	}
   ```
5. **`tree/expr/new.h`** (M3): dopo la riga 11 (`Decl *ctor_decl; …`)
   aggiungere
   `	std::string ctor_ident;  // simbolo del costruttore trovato (classe propria o antenata)`.
   Costruttore e distruttore di `NewNode` invariati.
6. **`tree/expr/new.cpp`** (M3):
   - `semant`: sostituire le righe 14-18
     ```cpp
     	// Check for constructor: classname_constructor (all lowercase)
     	if( ctor_args ){
     		StructType *st = sem_type->structType();
     		std::string ctor_name = st->ident + "_constructor";
     		ctor_decl = e->findFunc( ctor_name );
     ```
     con:
     ```cpp
     	// Cerca <classe>_constructor risalendo la catena superType, come il
     	// destructor in DeleteNode: i campi della madre stanno agli stessi indici
     	// nel figlio, quindi il costruttore della madre vale anche per il figlio
     	if( ctor_args ){
     		StructType *st = sem_type->structType();
     		ctor_decl = 0;
     		ctor_ident.clear();
     		for( StructType *c = st; c; c = c->superType ){
     			std::string name = c->ident + "_constructor";
     			Decl *d = e->findFunc( name );
     			if( d && (d->kind & DECL_FUNC) ){
     				ctor_decl = d;
     				ctor_ident = name;
     				break;
     			}
     		}
     ```
     Il resto di `semant` (r.20-53) invariato.
   - `translate` (x86) r.70-72: commento in
     `// simbolo del costruttore trovato in semant (classe propria o antenata)`
     (una riga) e `TNode *l = global( "_f" + ctor_ident );`.
   - `translate3` r.126:
     `std::string ctorName = g->toCSafeName( "f" + ctor_ident );`.
   - `translate2` (LLVM) invariato.
7. **Test** `test/language/class_ctor_inherit.bb` (nuovo), esattamente:
   ```blitz3d
   ; Costruttore ereditato dalla classe madre (finding M3) e Delete Null con destructor

   Global ctorInhDtorCount = 0

   Class CtorInhBase
   	Field a
   	Field name$
   	Method constructor(v)
   		Self\a = v
   		Self\name = "base" + v
   	End Method
   	Method destructor()
   		ctorInhDtorCount = ctorInhDtorCount + 1 + Self\a
   	End Method
   End Class

   Class CtorInhChild Extends CtorInhBase
   	Field b
   End Class

   Class CtorInhGrand Extends CtorInhChild
   	Field c
   End Class

   Class CtorInhOwn Extends CtorInhBase
   	Field d
   	Method constructor(v)
   		Self\a = v * 10
   		Self\d = v
   	End Method
   End Class

   Local ctorInhC.CtorInhChild = New CtorInhChild(5)
   ctorInhC\b = 9
   Expect ctorInhC\a = 5, "an inherited constructor sets a parent field"
   Expect ctorInhC\name = "base5", "an inherited constructor sets a parent string field"
   Expect ctorInhC\b = 9, "a child field is independent of the inherited constructor"

   Local ctorInhG.CtorInhGrand = New CtorInhGrand(7)
   Expect ctorInhG\a = 7, "a constructor is inherited across two levels"

   Local ctorInhO.CtorInhOwn = New CtorInhOwn(3)
   Expect ctorInhO\a = 30 And ctorInhO\d = 3, "a child constructor overrides the parent one"

   Local ctorInhN.CtorInhBase = Null
   Delete ctorInhN
   Expect ctorInhDtorCount = 0, "Delete Null does not run the destructor"
   ```
   `test/language.bb`: aggiungere `Include "language/class_ctor_inherit.bb"`
   subito dopo `Include "language/assignment.bb"` (r.4), prima di
   `Include "language/class_forward.bb"`.

Nessun'altra modifica.

## Verifiche obbligatorie

Nei comandi: `R=/home/rufio72/blitx3d_linux`, `T=$(mktemp -d)`,
`export blitzpath=$R/_release`, `B=$R/_release/bin/blitzcc`. Le uscite di
programmi `-r test` contengono codici colore ANSI: filtrare con
`sed 's/\x1b\[[0-9;]*m//g'`. Scenari (creare in `$T`, testo esatto):

`m1null.bb`:
```blitz3d
Class NullChk
	Field x
	Method Hello()
		Return 1
	End Method
End Class
Local okObj.NullChk = New NullChk()
Expect okObj\Hello() = 1, "method call on a live object"
Local nullObj.NullChk = Null
Expect nullObj\Hello() = 1, "method call on Null"
```
`m1del.bb`: come `m1null.bb` con classe `DelChk`, variabile `delObj`, e al
posto delle ultime due righe `Delete delObj` e
`Expect delObj\Hello() = 1, "method call after Delete"` (prima del Delete
`Expect delObj\Hello() = 1, "method call before Delete"`).
`m2s.bb`: classe `M2S` con `Field x` e `Static Method Constructor(a)` /
`Return a` / `End Method` (riga 3 = `Static Method …`), poi
`Local o.M2S = New M2S(7)`.
`m2d.bb`: classe `M2D` con `Field x` e `Static Method destructor()` (riga 3),
poi `Local o.M2D = New M2D()` e `Delete o`.
`m3err.bb`: le classi `CtorInhBase` e `CtorInhChild` del test del punto 7
(solo il costruttore, senza destructor) e `Local c.CtorInhChild = New CtorInhChild()`.

Compilazione degli scenari:
`cd $T && $B -q -r test [-d] -o $T/x_<nome> <nome>.bb && ./x_<nome> | sed 's/\x1b\[[0-9;]*m//g'`.

1. **Stato prima** (prima di toccare il codice, compilatore attuale):
   scenari e test del punto 7 come file isolato. Misurato dall'analista:
   - `m1null` con e senza `-d`: `PASS: method call on a live object.`,
     `PASS: method call on Null.`, `Pass: 2. Fail: 0`, rc 0.
   - `m1del` con e senza `-d`: `Pass: 2. Fail: 0` (anche la chiamata dopo
     `Delete` passa).
   - `m2s`: `"m2s.bb":7:1:7:1:Too many parameters`.
   - `m2d`: errore gcc `too many arguments to function 'bb_fm2d_destructor'`
     e `Failed to build executable`.
   - `class_ctor_inherit.bb` (senza le ultime 3 righe):
     `"class_ctor_inherit.bb":28:1:28:1:Constructor not found in class 'ctorinhchild'`
     (riga riferita alla versione misurata; col file del punto 7 cambia).
   - `Delete` di un Null con destructor che legge `Self\a`: segfault, rc 139,
     con e senza `-d`.
2. Compilazione: `ninja -C $R/build/x86_64-linux-release blitzcc` → nessun
   errore, `$B` con data nuova. (Non compila il codice sotto `USE_LLVM`.)
3. M1 debug, `m1null` con `-d` → `PASS: method call on a live object.`, poi
   una riga con `Object does not exist`, poi `Pass: 1. Fail: 0`; nessuna riga
   `method call on Null` (attesa, non misurata; il formato atteso dell'errore
   viene da `RuntimeError "boom"` con `-r test -d`, misurato:
   `:[<numeri>] boom`, poi `==== Results ====`, rc 0).
   Senza `-d`: invariato, `Pass: 2. Fail: 0` (attesa).
4. M1 cancellato, `m1del` con `-d` → `PASS: method call before Delete.`, riga
   con `Object does not exist`, `Pass: 1. Fail: 0` (attesa, non misurata).
5. M1 C generato: `grep -c "_bbNullChk(bb__l" $T/x_m1null.c` dopo la
   compilazione con `-d` → `2`; dopo quella senza `-d` (stesso nome di
   uscita, ricompilare) → `0` (attesa, non misurata). Il `.c` resta accanto
   all'eseguibile (`codegen_c.cpp:735-761`).
6. M2: `m2s` → errore a riga 3 con testo `constructor cannot be a Static Method`;
   `m2d` → riga 3, `destructor cannot be a Static Method`; nessuna chiamata a
   gcc (attesa, non misurata: le colonne possono variare).
7. M3: `$T/class_ctor_inherit.bb` copia del test del punto 7, compilato da
   solo → 6 righe `PASS:` e `Pass: 6. Fail: 0` (attesa, non misurata).
   `m3err` → `Not enough parameters` (attesa; stesso messaggio misurato oggi
   per `New OwnE()` sul costruttore proprio).
8. Suite e sample:
   ```
   cd $R/test && $B -q -r test -o $T/all_test all.bb && DISPLAY=:1 timeout 280 $T/all_test | tail -2
   ```
   → `Pass: 499. Fail: 17` (baseline 493 + 6 Expect; attesa). Poi:
   ```
   cd $T; for f in test_super test_inheritance test_multi_inheritance; do
     $B -q -r test -o $T/$f $R/_release/samples/oop/$f.bb && ./$f | tail -1; done
   for f in test_super test_inheritance test_multi; do
     diff $T/${f}_result.txt $R/_release/samples/oop/${f}_result.txt && echo "$f identico"; done
   ```
   → tre `Pass: 0. Fail: 0` e `test_super identico`, `test_inheritance identico`,
   `test_multi identico`.
9. Revisione: `git -C $R diff src/tools/compiler/` → solo gli hunk dei punti
   1-6. L'hunk LLVM di `method_call.cpp` coincide riga per riga con il punto 2
   (tab compresi). Riportare nel rapporto il diff integrale di
   `method_call.cpp`.
10. Preprocessore invariato:
    ```
    cd $R/src/tools/compiler; for f in tree/expr/method_call.cpp tree/expr/new.cpp tree/expr/new.h tree/stmt/delete.cpp tree/parser_decl.cpp codegen_c/codegen_c.cpp; do echo "$f $(grep -c '^#if' $f) $(grep -c '^#endif' $f)"; done
    ```
    → `method_call.cpp 4 4`, `new.cpp 2 2`, `new.h 3 3`, `delete.cpp 2 2`,
    `parser_decl.cpp 0 0`, `codegen_c.cpp 1 1` (misurato oggi). Inoltre
    `grep -n "_bbNullObjEx\|CreateIsNull" tree/expr/method_call.cpp` → righe
    tutte comprese tra `#ifdef USE_LLVM` (r.102) e il suo `#endif`.
11. Ambito: `git -C $R status --short` → rispetto alla fotografia iniziale solo
    `M` sui 6 file `src/tools/compiler/{codegen_c/codegen_c.cpp,tree/expr/method_call.cpp,tree/expr/new.cpp,tree/expr/new.h,tree/stmt/delete.cpp,tree/parser_decl.cpp}`,
    `M test/language.bb`, `?? test/language/class_ctor_inherit.bb`; nessun
    `*_result.txt` in `_release/samples/oop/`.
12. Soglia righe:
    `find $R/src/tools/compiler \( -name '*.cpp' -o -name '*.h' \) | xargs wc -l | awk '$1>800 && $2!="totale"'`
    → nessuna riga.

## Consegna

- Un commit con i soli file del fix e del test, messaggio in inglese:
  ```
  Compiler: debug null check on method calls, reject Static ctor/dtor, inherit constructor (M1-M3)

  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
  Nessun push.
- Rapporto di massimo trenta righe: uscite delle verifiche 1-12 (righe
  `Pass/Fail`, errori di M2, diff integrale di `method_call.cpp`, conteggi
  della 10, `git status`), deviazioni dal brief.

## Domande aperte

1. Check M1 anche nel backend LLVM, non compilabile né eseguibile in locale?
   Proposta: sì, con il testo del punto 2 (modello `ArrayVarNode::translate2`);
   in alternativa limitarlo al backend C e annotare M1 «solo C» nella review.
   «preferenza»
2. Estendere `_bbNullChk` ai field var del backend C
   (`FieldVarNode::translate3/load3/store3`), che oggi in debug fanno segfault
   su `Local t.T = Null : Print t\x` (misurato, rc 139, anche con `-d`)?
   Proposta: no in questo compito (riguarda anche i `Type` classici, fuori
   dai finding OOP); annotarlo come nuovo finding. «scelta di progetto»
3. Guardia `Delete Null` col destructor (punto 3) solo nel backend C?
   Proposta: sì, solo C; il backend LLVM ha lo stesso difetto
   (`DeleteNode::translate2` r.50-57) e resta annotato come finding.
   «preferenza»
4. Semantica del costruttore ereditato: con il punto 6 anche `New Figlio()`
   (parentesi vuote) chiama un `constructor()` senza parametri della madre, e
   `New Figlio()` con `constructor(v)` nella madre diventa errore
   `Not enough parameters` (oggi compila senza chiamare nulla). È lo stesso
   comportamento già in vigore per il costruttore proprio (misurato).
   Proposta: accettare. «scelta di progetto»
5. `DeleteNode::translate` (x86, r.35) usa `st->ident` invece di
   `dtor_ident`: incoerente con la risalita, ma il backend x86 rifiuta l'OOP.
   Proposta: lasciarlo, fuori ambito. «preferenza»
