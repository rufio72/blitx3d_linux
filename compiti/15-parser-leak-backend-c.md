# Compito 15: leak sui rami d'errore del parser e dettagli del backend C (finding M9, M10)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## Obiettivo

Quattro correzioni piccole e indipendenti nel compilatore `blitzcc`
(`src/tools/compiler/`, percorsi sotto relativi a questa cartella):

- **M9**: nei tre file del parser alcuni nodi dell'AST sono tenuti in
  puntatori grezzi mentre il codice può ancora lanciare `Ex`; vanno protetti
  con `a_ptr` come nel resto del parser.
- **M10a**: `DeleteEachNode::translate3` emette senza a capo e costruisce il
  nome del tipo a mano.
- **M10b**: `Field x = 100` dentro una `Class` viene accettato ma il valore
  non arriva mai nell'oggetto: va rifiutato con un errore chiaro (solo `Class`;
  `Type` invariato, vedi fatti).
- **M10c**: una chiamata a una funzione userlib (`.decls`) nel backend C
  genera un simbolo che non esiste e fallisce solo al link: va rifiutata in
  compilazione.

Vincoli assoluti:
- Nessuna modifica fuori da `/home/rufio72/blitx3d_linux`; dentro, solo i 5
  file elencati in "Struttura da realizzare". Fotografa `git status --short`
  all'inizio e confrontalo alla fine: file già modificati o non tracciati
  prima di iniziare non si toccano e non si committano.
- Commenti nel codice in italiano; i messaggi d'errore del compilatore in
  inglese come tutti gli altri.
- Nessun file di codice sopra le 800 righe (oggi nessun `.cpp`/`.h` del
  compilatore li supera; il più grande toccato è `parser.cpp`, 609 righe).
- Non cambiare il comportamento dei `Type` classici, del backend x86
  (`translate`) né di quello LLVM (`translate2`). Niente installazioni,
  niente push.
- Build: `ninja -C /home/rufio72/blitx3d_linux/build/x86_64-linux-release blitzcc`
  (la build locale è senza LLVM: `USE_LLVM` spento, `USE_GCC_BACKEND` acceso).
  `blitzcc` vuole `-o` PRIMA del sorgente e la variabile
  `blitzpath=/home/rufio72/blitx3d_linux/_release`. Nei `.bb` di prova usare
  `WriteStdout` (`Print` scrive nella finestra grafica).

## Cosa esiste già (fatti verificati)

### `a_ptr` (da lettura, `src/stdutil/stdutil.h:66-80`)

```cpp
template<class T> class a_ptr{
	a_ptr(T *t=0); ~a_ptr(){delete t;}
	a_ptr &operator=(T *t){this->t=t;return *this;}   // NON cancella il vecchio
	T *operator->()const; operator T*()const; T *release();
};
```
Conseguenze: assegnare a un `a_ptr` non vuoto perde il vecchio puntatore
(fare sempre `release()` prima, come in `parser_expr.cpp:56-57`);
`dynamic_cast<X*>( p )` su un `a_ptr` NON compila (l'operando deve essere un
puntatore): scrivere `dynamic_cast<X*>( (ExprNode*)p )`.
Distruttori: `VarExprNode` cancella `var`; `MethodCallNode` cancella `obj` ed
`exprs`; `ExprSeqNode`, `StmtSeqNode`, `DeclSeqNode` cancellano i figli.
`Parser::ex`/`exp` (`parser.cpp:68-88`) lanciano sempre `Ex`.

### M9: mappa dei rami (da lettura integrale dei tre file)

| File:righe | Oggetto grezzo vivo | Punti che lanciano mentre è vivo |
|---|---|---|
| `parser.cpp:241-254` (statement `Self`/`This`) | `ExprNode *expr_or_var` | r.247 `ex("Unexpected expression")` (irraggiungibile: `parseVarOrMethodCall` rende solo `MethodCallNode` o `VarExprNode`), r.248 `exp("variable assignment")`, r.249 `parseExpr` |
| `parser.cpp:289-305` (statement chiamata con parentesi) | `ExprSeqNode *exprs` | r.302 `exp("')'")` dopo `parseExprSeq` di r.301 |
| `parser.cpp:310-345` (statement var/metodo) | `ExprNode *expr_or_var` | r.318 (irraggiungibile, come sopra), r.319 `exp`, r.320 `parseExpr` |
| `parser_expr.cpp:104-124` (`parseVarOrMethodCall`, catena di metodi) | `ExprNode *method_call` | r.110 `parseIdent`, r.111 `parseTypeTag` (sul `.` chiama `parseIdent`), r.115 `parseExprSeq`, r.116 `exp`, r.121 `ex("Field access after method call not yet supported")` |
| `parser_expr.cpp:322-325` (`New C(args)`) | `ExprSeqNode *args` | r.323 `exp("')'")` |

Controllati e senza leak (nessun lancio tra allocazione e consegna al
padrone): `parser.cpp` r.137-139, 188-191, 274-275, 306-307, 358-363,
368-374, 379-402, 407-433, 470-474, 479-482, 494-495, 521-523, 529-532,
539-542, 584-608 (`parseIf`); `parser_expr.cpp` r.40-69, 75-82, 101,
128-136, 145-152, 158-159, 166-241, 246-299, 304-316, 400-403, 416-419;
`parser_decl.cpp` intero (r.20-35 `expr` passa subito a `VarDeclNode`,
r.127-128, 134-139, 168-170 inseriti subito in un `a_ptr<DeclSeqNode>`).
`parser_decl.cpp` quindi non si tocca per M9.
Fuori ambito (non sono leak, il parse si interrompe comunque): in
`parseMethodDecl` `currentClassName` non viene ripristinato se
`parseStmtSeq` lancia (r.185-188); in `INCLUDE` `toker`/`incfile` idem.

### M10a (da lettura e misura)

- `tree/stmt/delete_each.cpp:24-26`:
  `g->emit( "_bbObjDeleteEach((BBObjType*)&_tbb_" + typeIdent + ");" );`
- `codegen_c/codegen_c.cpp:524-533`: `emit` scrive il testo e basta;
  `emitLine` aggiunge l'indentazione e `"\n"`.
- `codegen_c.cpp:589-600` `toCSafeName`: sostituisce i caratteri non validi
  con `_` e antepone `bb_`; quindi `"_t"+toCSafeName(x)` = `"_tbb_"+x` oggi.
  Lo stesso schema `"_t" + g->toCSafeName( ident )` è usato in
  `expr/first.cpp:28`, `stmt/for_each.cpp:107`, `expr/new.cpp:116`.
- Misura oggi (`-a` stampa il C): la riga generata per `Delete Each TP` è
  `_bbObjDeleteEach((BBObjType*)&_tbb_tp);    bbWriteStdout(...` cioè lo
  statement successivo finisce sulla stessa riga.

### M10b (da lettura e misura)

- Il parser accetta `= expr` per ogni `Field`: `parseVarDecl`
  (`parser_decl.cpp:29-33`) è chiamato da `parseStructDecl` (r.86) e da
  `parseClassDecl` (r.127, `DECL_FIELD | accessMod`).
- `VarDeclNode::proto` (`decl/var_decl.cpp:7-33`) fa semant e cast di `expr`
  e crea `sem_var`, ma per i campi nessuno lo usa: `NewNode::translate3`
  (`expr/new.cpp:114-137`) e `translate2` (r.82-108) chiamano solo
  `_bbObjNew` (+ costruttore); `StructDeclNode::translate3`/`ClassDeclNode`
  non traducono i campi. Il backend x86 (`StructDeclNode::translate`,
  `decl/struct_decl.cpp:22`) chiama `fields->translate` →
  `VarDeclNode::translate` con un `DeclVarNode` di tipo campo: codice
  privo di senso, solo Windows 32 bit, non verificato.
- Misurato oggi (backend C): Type con `Field x = 100` / `Field name$ = "abc"`
  stampa `type x=0 name=[]`; Class con `Private Field v = 42` stampa `v=0`.
- Codice Blitz3D reale usa gli inizializzatori nei `Type` contando che siano
  ignorati: `/home/rufio72/scpcb-master/*.bb` ne ha 15 (es. `Main.bb:1858`
  `Field MTFClose% = True`, `MapSystem.bb:1631` `Field DisableOverlapCheck% = True`).
  Applicarli o rifiutarli nei `Type` cambierebbe o romperebbe SCP-CB: i
  `Type` restano come sono. Nel repo nessun `.bb` usa inizializzatori di
  campo (grep).
- Costo di applicarli davvero nei `Class` (valutato, NON da fare qui): servirebbe
  una lista (indice campo, `ExprNode*`) su `StructType`, copiata dal padre in
  `ClassDeclNode::semant` (`decl/class_decl.cpp:31-57`) per i campi
  ereditati; un temporaneo e gli store in tre backend (`NewNode::translate`
  x86 con `sem_temp`, `translate2` LLVM non compilabile in locale,
  `translate3` C); e l'espressione è semanticata in `proto` dei campi, prima
  di Global e Function del programma. Per questo il compito prescrive
  l'errore (domanda aperta 1).

### M10c (da lettura e misura)

- `libs.cpp:150-238` `loadUserLib` registra ogni funzione dei `.decls` con
  `d_new FuncType( ty,params,true,true )` (`userlib=true`, `symbol` vuoto) e
  la mette in `userFuncs`. `libs.cpp:240-266` `linkUserLibs`: il caricamento
  dei `.decls` esiste solo sotto `#ifdef WIN32` (r.244-262); altrove è vuoto.
- `main.cpp:340-348`: senza LLVM, o su `BB_WIN32`, il backend di default è
  quello C (`usegcc=true`). Quindi su Windows i `.decls` vengono caricati e
  una chiamata userlib arriva a `CallNode::translate3`
  (`expr/call.cpp:162-186`): `f->symbol` è vuoto, quindi diventa
  `bb_f<nome>(...)`, mai definito → errore solo al link.
  `ProgNode::translate3` (`prognode.cpp:279`) riceve `userfuncs` e non li usa.
- `FuncType` (`tree/type.h:49-54`) ha il campo `bool userlib`.
- Gli `Ex` lanciati dentro un'espressione in `translate3` prendono la
  posizione dello statement: `StmtSeqNode::translate3`
  (`stmt/stmt_seq.cpp:93-105`) li riempie con `stmts[k]->pos` e `file`;
  `main.cpp:613-617` li stampa come `"file":riga:col:...:messaggio`.
- Su Linux (locale) il percorso è irraggiungibile: misurato oggi,
  `x = FakeUserFn(3)` dà `"userlib.bb":1:1:1:1:Function 'fakeuserfn' not found`
  (anche con un `.decls` in `$blitzpath/userlibs`, che su Linux non viene letto).

## Struttura da realizzare

### 1. `src/tools/compiler/tree/parser.cpp` (M9)

a) Statement `Self`/`This`, r.241-254: sostituire con

```cpp
				a_ptr<ExprNode> expr_or_var( parseVarOrMethodCall( "self","",true ) );
				if( dynamic_cast<MethodCallNode*>( (ExprNode*)expr_or_var ) ){
					// Method call as statement: Self\Method() / Self.Method()
					result=d_new ExprStmtNode( expr_or_var.release() );
				}else{
					VarExprNode *var_expr=dynamic_cast<VarExprNode*>( (ExprNode*)expr_or_var );
					if( !var_expr ) ex( "Unexpected expression" );
					if( toker->curr()!='=' ) exp( "variable assignment" );
					toker->next();ExprNode *expr=parseExpr( false );
					VarNode *var=var_expr->var;
					var_expr->var=0; // prevent double delete
					delete expr_or_var.release();
					result=d_new AssNode( var,expr );
				}
```
b) Statement chiamata, r.289: `a_ptr<ExprSeqNode> exprs;` al posto di
`ExprSeqNode *exprs;` (le tre assegnazioni r.301/304/305 restano: l'`a_ptr`
è vuoto); r.306 `CallNode *call=d_new CallNode( ident,tag,exprs.release() );`.
c) Statement var/metodo, r.310-345: stesso schema di a):
`a_ptr<ExprNode> expr_or_var( parseVarOrMethodCall( ident,tag ) );`, i due
`dynamic_cast` con `(ExprNode*)expr_or_var`, r.314
`d_new ExprStmtNode( expr_or_var.release() )`, r.343 `delete var_expr;` →
`delete expr_or_var.release();`. Il resto (inferenza del tag, r.322-341)
invariato.

### 2. `src/tools/compiler/tree/parser_expr.cpp` (M9)

a) `parseVarOrMethodCall`, r.104-124:
```cpp
				a_ptr<ExprNode> method_call( d_new MethodCallNode( obj_expr,field_ident,field_tag,exprs.release() ) );
```
dentro il ciclo, al posto di r.118:
```cpp
						ExprNode *prev=method_call.release();
						method_call=d_new MethodCallNode( prev,next_ident,next_tag,next_exprs.release() );
```
r.124 `return method_call.release();`. Commenti esistenti invariati.
b) `parsePrimary`, `case BBNEW`, r.322-325:
`a_ptr<ExprSeqNode> args( parseExprSeq() );` e
`result=d_new NewNode( t,args.release() );`.

### 3. `src/tools/compiler/tree/stmt/delete_each.cpp` (M10a)

r.25 diventa:
```cpp
	g->emitLine( "_bbObjDeleteEach((BBObjType*)&_t" + g->toCSafeName( typeIdent ) + ");" );
```
Non aggiungere `#ifdef` né toccare `translate`/`translate2`.

### 4. `src/tools/compiler/tree/parser_decl.cpp` (M10b)

In `parseClassDecl`, ramo `FIELD` (r.125-129), dopo
`fields->push_back( fieldDecl );` (prima il push, così l'`a_ptr` `fields`
possiede il nodo quando si lancia):
```cpp
				// New non applica gli inizializzatori dei campi: nei Class si
				// rifiutano (nei Type restano ignorati come in Blitz3D)
				VarDeclNode *vd=dynamic_cast<VarDeclNode*>( fieldDecl );
				if( vd && vd->expr ) ex( "Field initializers are not supported in Class; assign the field in Method Constructor()" );
```
`decl/var_decl.h` è già incluso (r.11). `parseStructDecl` non si tocca.

### 5. `src/tools/compiler/tree/expr/call.cpp` (M10c)

In `CallNode::translate3`, subito dopo `FuncType *f = sem_decl->type->funcType();`
(r.163):
```cpp
	// le userlib (.decls) non hanno un simbolo nel backend C: meglio un
	// errore qui che un undefined symbol al link
	if( f->userlib ) ex( "Userlib function '"+ident+"' is not supported by the C backend" );
```
`translate`/`translate2` invariati.

Nient'altro: nessun test nuovo in `test/` (gli errori di compilazione non si
provano con la suite).

## Verifiche obbligatorie

Nei comandi: `R=/home/rufio72/blitx3d_linux`, `T=$(mktemp -d)`,
`export blitzpath=$R/_release`, `B=$R/_release/bin/blitzcc`.
Sui leak (M9): senza valgrind/ASan (non installati) non sono osservabili;
le verifiche di M9 sono compilazione + messaggi d'errore identici + non
regressione.

1. Stato PRIMA (eseguire col compilatore attuale, prima di modificare).
   Creare in `$T` i file:
   - `e1_self.bb`: `Class C` / `Field a` / `Method M()` / `Self\a + 1` / `End Method` / `End Class`
   - `e2_call.bb`: `Function Foo(a, b)` / `End Function` / `Foo(1 2)`
   - `e3_var.bb`: `Type T` / `Field y` / `End Type` / `x.T = New T` / `x\y + 1`
   - `e4_chain.bb`: `Class C` / `Field f` / `Method M.C()` / `Return Self` / `End Method` / `End Class` / `o.C = New C` / `r = o\M()\f`
   - `e5_new.bb`: `Class C` / `Field f` / `End Class` / `o.C = New C(1 2)`
   - `e6_selfrhs.bb`: come `e1_self.bb` con `Self\a = )` al posto di `Self\a + 1`
   - `fi_type.bb`: `Type TPoint` / `Field x = 100` / `Field name$ = "abc"` / `End Type` / `p.TPoint = New TPoint` / `WriteStdout "type x=" + p\x + " name=[" + p\name + "]"`
   - `fi_class.bb`: `Class CItem` / `Field a` / `Private Field v = 42` / `Method Show()` / `WriteStdout "v=" + Self\v` / `End Method` / `End Class` / `c.CItem = New CItem` / `c\Show()`
   - `de.bb`: `Type TP` / `Field y` / `End Type` / `x.TP = New TP` / `Delete Each TP` / `WriteStdout "n=" + (First TP = Null)`
   - `userlib.bb`: `x = FakeUserFn(3)`

   (`/` separa le righe.) Poi:
   ```
   cd $T; for f in e1_self e2_call e3_var e4_chain e5_new e6_selfrhs; do $B -q -o $T/o_$f $f.bb 2>&1 | tail -1; done
   for f in fi_type fi_class; do $B -q -o $T/o_$f $f.bb >/dev/null 2>&1; DISPLAY=:1 $T/o_$f 2>/dev/null | grep -v '^GL '; done
   $B -q -a -o $T/o_de de.bb 2>&1 | grep -c '_bbObjDeleteEach((BBObjType\*)&_tbb_tp);    bbWriteStdout'
   $B -q -o $T/o_ul userlib.bb 2>&1 | tail -1
   ```
   Uscita misurata oggi:
   ```
   "e1_self.bb":4:10:4:10:Expecting variable assignment
   "e2_call.bb":3:7:3:7:Expecting ')'
   "e3_var.bb":5:5:5:5:Expecting variable assignment
   "e4_chain.bb":8:12:8:12:Field access after method call not yet supported
   "e5_new.bb":4:15:4:15:Expecting ')'
   "e6_selfrhs.bb":4:12:4:12:Expecting expression
   type x=0 name=[]
   v=0
   1
   "userlib.bb":1:1:1:1:Function 'fakeuserfn' not found
   ```
   (Le righe `e*` sono misurate con indentazione a tab; con altra
   indentazione cambiano le colonne, non i messaggi.)
2. Compilazione: `ninja -C $R/build/x86_64-linux-release blitzcc` → nessun
   errore; `$B` ha data nuova (se `_release/bin/blitzcc` non viene
   aggiornato dalla build, copiarlo dalla cartella di build come fanno i
   compiti precedenti e dirlo nel rapporto).
3. M9, messaggi invariati: ripetere il primo ciclo del punto 1 → le sei righe
   `e*` identiche, carattere per carattere.
4. M10a: `$B -q -a -o $T/o_de $T/de.bb 2>&1 | grep -E '^\s*_bbObjDeleteEach\(\(BBObjType\*\)&_tbb_tp\);$'`
   → una riga (lo statement da solo sulla sua riga, indentato); e
   `DISPLAY=:1 $T/o_de 2>/dev/null | grep -v '^GL '` → `n=1` (misurato oggi).
5. M10b: `fi_type` → ancora `type x=0 name=[]` (Type invariato);
   `cd $T && $B -q -o $T/o_fic fi_class.bb 2>&1 | tail -1` →
   `"fi_class.bb":3:<col>:3:<col>:Field initializers are not supported in Class; assign the field in Method Constructor()`
   (attesa, non misurata: riga 3; la colonna è quella del token dopo `42`).
6. M10c: `userlib.bb` → ancora `Function 'fakeuserfn' not found` (su Linux i
   `.decls` non si caricano). Il ramo nuovo non è eseguibile in locale:
   `grep -n "f->userlib" $R/src/tools/compiler/tree/expr/call.cpp` → due
   righe (r.126 preesistente in `translate` e quella nuova in `translate3`).
7. Sample OOP invariati, eseguiti in `$T`:
   ```
   cd $T; for f in test_super test_inheritance test_multi_inheritance; do
     $B -q -r test -o $T/$f $R/_release/samples/oop/$f.bb && ./$f | tail -1; done
   for f in test_super test_inheritance test_multi; do
     diff $T/${f}_result.txt $R/_release/samples/oop/${f}_result.txt && echo "$f identico"; done
   ```
   → tre `Pass: 0. Fail: 0` e `test_super identico`, `test_inheritance identico`,
   `test_multi identico`.
8. Suite: `cd $R/test && $B -q -r test -o $T/all_test all.bb && DISPLAY=:1 timeout 280 $T/all_test | tail -2`
   → `Pass: 493. Fail: 17` (baseline; i 17 sono ambientali).
9. Ambito: `git -C $R status --short` → rispetto alla fotografia iniziale solo
   ` M` su `src/tools/compiler/tree/parser.cpp`, `parser_expr.cpp`,
   `parser_decl.cpp`, `stmt/delete_each.cpp`, `expr/call.cpp`. Nessun file
   scritto in `_release/samples/oop/` né in `_release/userlibs/`.
10. Soglia righe:
    `find $R/src/tools/compiler \( -name '*.cpp' -o -name '*.h' \) | xargs wc -l | awk '$1>800 && $2!="totale"'`
    → nessuna riga.

## Consegna

- Un commit con i soli 5 file sopra, messaggio in inglese:
  ```
  Compiler: a_ptr on parser error paths, C backend details (M9, M10)

  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
  Nessun push.
- Rapporto finale di massimo trenta righe: uscita delle verifiche 1-10
  (righe misurate, `Pass/Fail`, `git status`), deviazioni dal brief.

## Domande aperte

1. M10b, Field con inizializzatore nei `Class`: errore in compilazione
   (proposta) oppure applicazione del valore nel `New`. Proposta: errore.
   Motivi: applicarlo tocca `StructType`, `ClassDeclNode::semant` (campi
   ereditati) e `NewNode` in tre backend, con LLVM non compilabile in
   locale, e l'espressione andrebbe semanticata in un contesto diverso da
   quello di oggi; l'errore è di 3 righe, provabile, e si può togliere il
   giorno in cui la funzione si implementa. I `Type` restano ignorati in
   silenzio in ogni caso, perché SCP-CB ne dipende. «scelta di progetto»
2. Testo dei due messaggi nuovi (`Field initializers are not supported in
   Class; assign the field in Method Constructor()`, `Userlib function 'x'
   is not supported by the C backend`). Proposta: come scritti. «preferenza»
3. M10c: il controllo è in `CallNode::translate3`, che su Linux non si può
   esercitare (i `.decls` si caricano solo su `WIN32`). Alternativa: far
   caricare i `.decls` anche su Linux/macOS (scansione di
   `$blitzpath/userlibs` in `linkUserLibs`) per renderlo provabile, ma
   cambia il perimetro e fa leggere file finora ignorati. Proposta: solo il
   controllo, verifica per lettura. «preferenza»
