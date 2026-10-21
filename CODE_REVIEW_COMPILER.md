# Code review statica — src/tools/compiler

Data: 2026-10-21 · Base: commit `d8d5d1b` → `HEAD` (5bb2bd8) · ~140 file toccati, +4363/−167 righe.

## Sintesi

La review copre le modifiche al compilatore dal dicembre 2025: l'estensione OOP
(Class/Method/Self/Super/Extends/Static, metodi compilati come funzioni globali
`ClassName_MethodName(self,…)`), il nuovo backend C ("GCC backend", oggi il
default), gli aggiornamenti a codegen LLVM, linker LLD/GCC e packager APK.
Tre difetti critici: (1) sul backend x86 legacy (win32) tutte le chiamate OOP
sono generate con un nodo `IR_ARG` improprio che scarta oggetto e argomenti —
qualunque programma OOP a 32 bit è rotto; (2) i corpi dei metodi sono analizzati
prima della registrazione delle Function utente e delle Global: le funzioni
"non esistono" e le Global diventano silenziosamente locali del metodo (dati
persi senza errore); (3) nel backend C il confronto tra oggetti usa `==` sui
puntatori invece di `_bbObjCompare`, quindi l'idioma `If o = Null` dopo `Delete`
dà il risultato sbagliato. Seguono 9 finding importanti (controllo accessi
aggirabile, firme dei metodi senza forward reference, nuove parole riservate che
rompono i sorgenti classici, leak e doppie valutazioni nel backend C, ecc.) e
10 minori. Il backend LLVM è quello messo meglio; il grosso dei problemi sta
nel percorso x86+OOP e nel backend C nuovo.

## Perimetro esaminato

- Priorità: file toccati da `git diff d8d5d1b..HEAD -- src/tools/compiler`,
  letti nello stato attuale per intero: `tree/parser.cpp`, `tree/toker.cpp`,
  `tree/decl/class_decl.*`, `tree/expr/method_call.*`, `tree/expr/new.*`,
  `tree/stmt/delete.*`, `tree/var/*` (field, vector, decl, array, node),
  `tree/prognode.cpp`, `tree/decl/func_decl.cpp`, `tree/decl/struct_decl.cpp`,
  `tree/decl/var_decl.cpp`, `tree/decl/vector_decl.cpp`, `tree/decl/data_decl.cpp`,
  tutti gli `stmt/*.cpp` ed `expr/*.cpp` con i nuovi `translate3`,
  `codegen_c/*` (backend C completo), `linker_gcc/*`, `linker_lld/linker_lld.cpp`,
  `codegen_llvm/codegen_llvm.cpp`, `main.cpp`, `environ.*`, `libs.cpp`,
  `package/package_apk.cpp`, `CMakeLists.txt`, `tree/type.*`, `tree/decl.*`.
- Il codegen/linker x86 ereditato è stato letto solo dove le modifiche OOP vi
  passano attraverso (`munchCall`/`munchReg`, `ExprSeqNode::translate`).
- Nessuna build eseguita (serve LLVM 19): verifica per sola lettura, con
  controllo incrociato sul runtime (`src/modules/bb/blitz/basic.cpp`) dove
  serviva per decidere la semantica attesa.
- Convenzione: "da verificare" marca i finding plausibili ma non confermabili
  senza eseguire il compilatore.

---

## Critici

### C1. Backend x86 (win32): tutte le chiamate OOP generano codice senza argomenti

- `tree/expr/method_call.cpp:86` e `:221`, `tree/expr/new.cpp:68`,
  `tree/stmt/delete.cpp:25`.
- Le chiamate a metodi, `Super`, costruttori e distruttori costruiscono il nodo
  argomenti come `d_new TNode( IR_ARG, obj_node, args_node, 0 )`. Ma nel
  codegen x86 `IR_ARG` è una foglia: `munchReg` (`codegen_x86/codegen_x86.cpp:384`)
  emette solo `lea %l,[esp+0]` e **non visita i figli**. Risultato: l'espressione
  oggetto non viene mai valutata e nessun argomento viene scritto sullo stack;
  la funzione è chiamata con spazzatura in `[esp+…]`.
- Anche correggendo la forma (come fa `Node::call`: `move(obj, mem(arg(0)))` in
  sequenza), gli offset prodotti da `ExprSeqNode::translate`
  (`tree/expr/expr_seq.cpp:32`, `k*4`) colliderebbero con lo slot 0 di `self`:
  serve un offset base (+4) per gli argomenti.
- Scenario: qualunque `.bb` con `obj\Metodo()`, `New Classe(args)` o `Delete`
  con destructor compilato per win32 a 32 bit (unico uso rimasto del codegen
  x86) → crash o risultati casuali a runtime.
- Correzione suggerita: costruire la sequenza argomenti con il pattern di
  `Node::call` (store di `self` in `mem(arg(0))`, poi gli argomenti con offset
  shiftati di 4, p.es. aggiungendo un parametro "offset base" a
  `ExprSeqNode::translate`), oppure dichiarare esplicitamente l'OOP non
  supportato sul backend x86 con un errore in fase di translate.

### C2. Ordine di semant: i corpi dei metodi non vedono Function utente, Global e Dim

- `tree/prognode.cpp:15-22`: l'ordine è `structs->proto` → `structs->semant`
  (che include `methods->semant`, `tree/decl/class_decl.cpp:50`) →
  `funcs->proto` → `stmts->semant`.
- Dentro un `Method`:
  - una chiamata a una `Function` utente fallisce con "Function 'x' not found"
    (le Function sono registrate solo dopo, in `funcs->proto`); le Function
    normali invece vedono tutto perché il loro `semant` gira per ultimo;
  - un riferimento a una `Global` (o a un array `Dim`) dichiarata nel programma
    principale non trova la decl (inserita in `stmts->semant`, che gira dopo) e
    per una Global scatta l'"ugly auto decl" di
    `tree/var/ident_var.cpp` → viene creata una **Local del metodo** con lo
    stesso nome: letture che danno 0 e scritture perse, **senza alcun errore**.
- Scenario:
  ```blitz3d
  Global punteggio
  Class Gioco
    Method Aggiungi()
      punteggio = punteggio + 1   ; crea una local "punteggio" nel metodo: la Global resta 0
    End Method
  End Class
  ```
  Idem `Method M() : Aiuto() : End Method` con `Function Aiuto()` definita
  altrove → errore di compilazione spurio.
- Correzione suggerita: separare il semant dei corpi dei metodi in una passata
  dedicata eseguita dopo `funcs->proto` e `stmts->semant` (p.es.
  `structs->semantMethods(env)` chiamata tra `stmts->semant` e `funcs->semant`),
  lasciando in `structs->semant` solo campi e offset.

### C3. Backend C (default): confronto tra oggetti con `==` sui puntatori invece di `_bbObjCompare`

- `tree/expr/rel_expr.cpp:118` (`translate3`): solo le stringhe passano per
  `_bbStrCompare`; gli oggetti cadono nel ramo generico
  `(bb_int_t)((l) == (r))`.
- La semantica classica (x86 `Node::compare` → `__bbObjCompare`, LLVM
  `compare2` → `_bbObjCompare`) confronta `o->fields`: un oggetto passato a
  `Delete` ha `fields==0` e quindi **è uguale a Null**
  (`src/modules/bb/blitz/basic.cpp:362`). Col confronto raw il puntatore
  "zombie" resta diverso da 0.
- Scenario (idioma onnipresente nel codice Blitz):
  ```blitz3d
  Local e.Enemy = New Enemy
  Delete e
  If e = Null Then Print "morto"   ; backend C: NON stampa; x86/LLVM: stampa
  ```
  Divergono anche i confronti tra due riferimenti allo stesso oggetto deletato.
- Correzione suggerita: in `RelExprNode::translate3`, se `opType->structType()`
  generare `(bb_int_t)(_bbObjCompare((bb_obj_t)l,(bb_obj_t)r) cmpOp 0)`
  (dichiarazione già presente nell'header generato).

---

## Importanti

### I1. Controllo accessi Private/Protected aggirabile e incoerente

- `environ.cpp:69` (`getCurrentClass`): il "contesto di classe" è dedotto
  dall'esistenza di un **parametro chiamato `self`**. Una Function normale con
  un parametro di nome `self` ottiene pieno accesso ai membri privati:
  ```blitz3d
  Function Ruba%(self.Cassaforte)
    Return self\codiceSegreto   ; campo Private: accesso concesso
  End Function
  ```
- `tree/var/field_var.cpp:14-27`: il check Private confronta con il **tipo
  statico dell'espressione** (`s`), non con la classe proprietaria del campo:
  un campo `Private` ereditato è accessibile dalla sottoclasse (i campi sono
  copiati nel figlio da `ClassDeclNode::semant`, quindi `s == currentClass`).
  Specularmente il check Protected usa `isSubclassOf(s)`: un metodo del padre
  che riceve un oggetto con tipo statico della sottoclasse viene rifiutato.
- Gli `Static Method` non hanno `self` → non accedono nemmeno ai privati della
  **propria** classe.
- `SuperMethodCallNode::semant` non fa alcun check di accesso (un metodo
  `Private` del padre è chiamabile via `Super`).
- Correzione suggerita: memorizzare `ownerClass` anche sui `Decl` dei campi
  (c'è già il membro, usato solo per i metodi) e sostituire l'euristica di
  `getCurrentClass` con un vero contesto di classe impostato dal semant dei
  metodi (p.es. un campo in `Environ`).

### I2. Le firme dei metodi non ammettono forward reference a classi/Type successivi

- `tree/decl/class_decl.cpp:24` (`methods->proto(e->funcDecls,e)` dentro
  `ClassDeclNode::proto`) + `tree/decl/func_decl.cpp:7`
  (`tagType`/`params->proto` risolvono subito i tipi).
- Il proto dei metodi gira durante `structs->proto`, in ordine di
  dichiarazione: un metodo che ritorna o riceve una classe dichiarata **dopo**
  fallisce con "Type \"b\" not found", mentre una `Function` identica compila
  (le Function sono protoate dopo tutte le struct).
  ```blitz3d
  Class A
    Method Crea.B()        ; errore: Type "b" not found
    End Method
  End Class
  Class B
  End Class
  ```
- Correzione suggerita: spostare `methods->proto` in una seconda passata sulle
  classi, dopo che `structs->proto` ha registrato tutti i tipi.

### I3. Dieci nuove parole riservate globali rompono i sorgenti classici

- `tree/toker.cpp:38-49`: `Class`, `Method`, `Super`, `Extends`, `Static`,
  `Private`, `Public`, `Protected` (più `End Class`/`End Method`) sono token
  riservati ovunque. Solo `Self`/`This` sono stati resi contestuali
  (`parser.cpp:137-144, 247-252, 1148-1152`).
- Scenario: codice Blitz3D classico con `Local static = 5`, `Function class()`,
  `Global method` … non compila più ("Expecting identifier"). Sono nomi comuni
  nei progetti esistenti che questo fork dichiara di voler supportare.
- Correzione suggerita: rendere contestuali anche queste keyword (riconoscerle
  solo a inizio istruzione in scope programma / dentro `Class … End Class`),
  con lo stesso meccanismo usato per Self/This.

### I4. Backend C: `_bbSgn` non dichiarato → errore di compilazione con GCC ≥ 14

- `tree/expr/uni_expr.cpp:90` genera `_bbSgn(…)` per `Sgn` su interi, ma
  l'header fisso di `codegen_c/codegen_c.cpp` dichiara `_bbAbs` (riga 129) e
  **non** `_bbSgn`; `declareRuntimeFunc` è invocato solo da
  `CallNode::translate3`. Il simbolo esiste nel runtime
  (`src/modules/bb/blitz/module.link.cpp:59`), quindi il link passerebbe, ma la
  compilazione C produce una implicit declaration: errore con GCC 14+/C23 (e
  tipo di ritorno `int` 32-bit implicito su `bb_int_t` 64-bit con compilatori
  più vecchi).
- Scenario: `Print Sgn(-5)` con il backend di default.
- Correzione suggerita: aggiungere `extern bb_int_t _bbSgn(bb_int_t);`
  all'header generato (e, per coerenza, far passare anche questi helper da
  `declareRuntimeFunc`).

### I5. Backend C: inizializzatori di variabile senza store → leak e incoerenza

- `tree/decl/var_decl.cpp:66-75` (`translate3`): `Local s$ = espr` è emesso
  come assegnamento raw `_ls = valore;`, mentre x86 e LLVM passano da
  `sem_var->store/store2` (quindi `_bbStrStore`/`_bbObjStore` con release del
  valore precedente).
- Scenario: `While …: Local s$ = ReadLine(f): Wend` — a ogni giro la stringa
  precedente non viene rilasciata: leak proporzionale alle iterazioni (il
  cleanup di fine funzione rilascia solo l'ultima).
- Correzione suggerita: usare `sem_var->store3(g,value)`.

### I6. Delete: espressione valutata due volte quando esiste un destructor

- Backend C, `tree/stmt/delete.cpp:57-69`: la stringa C dell'espressione è
  incollata due volte (chiamata al destructor e `_bbObjDelete`), quindi i side
  effect girano due volte. Backend x86, `tree/stmt/delete.cpp:28`: dopo la
  chiamata (comunque rotta, v. C1) l'albero fa `t = expr->translate(g)` una
  seconda volta.
- Scenario: `Delete PopNemico()` con `Method destructor()` definito → la
  funzione è chiamata due volte; il secondo valore viene deletato, il primo
  resta con il destructor eseguito.
- Correzione suggerita: nel backend C valutare in un temp
  (`bb_obj_t t = expr; dtor(t); _bbObjDelete(t);`); il backend LLVM lo fa già
  correttamente.

### I7. Destructor non ereditato e mai chiamato nelle altre vie di distruzione

- `tree/stmt/delete.cpp:13-15`: il lookup è `st->ident + "_destructor"` sul
  solo tipo statico, senza risalire `superType` (i metodi invece risalgono la
  catena in `MethodCallNode::semant`).
- Scenari: `Class B Extends A` con destructor solo in `A` → `Delete b` non
  chiama `A_destructor`; `Delete Each T` e il rilascio per refcount
  (`_bbObjRelease`) non chiamano mai alcun destructor; con tipo statico padre e
  oggetto figlio viene chiamato il destructor del padre (nessun dispatch).
- Correzione suggerita: risalire la catena come per i metodi e documentare
  esplicitamente che il destructor gira solo su `Delete` esplicito.

### I8. `Super` dentro `Static Method` aggancia qualunque `self` visibile

- `tree/expr/method_call.cpp:188`: `self_decl = e->findDecl("self")` senza
  verificare kind né tipo. In uno `Static Method` (il parser imposta comunque
  `currentClassName`, quindi `Super` è accettato) il lookup può trovare una
  `Global self` del programma: su x86 `local(self_decl->offset)` legge un
  offset privo di senso, su LLVM si carica un valore del tipo sbagliato da
  passare come `self`. Senza una global `self` l'errore c'è ma è fuorviante
  ("Super can only be used inside a method").
- Correzione suggerita: in parse rifiutare `Super`/`Self` negli Static Method;
  in semant esigere `kind & DECL_PARAM` e tipo struct per `self_decl`.
  (Il caso "global self" è **da verificare** a runtime, la lettura del codice è
  univoca ma lo scenario è raro.)

### I9. (da verificare) LLVM: metodi che toccano campi di classi dichiarate dopo → GEP su struct opaco

- I metodi sono tradotti dentro `ClassDeclNode::translate2`
  (`tree/decl/class_decl.cpp:175`), cioè durante `structs->translate2`; il
  body llvm di una classe è impostato (`setBody`) solo nel **suo** translate2
  (`class_decl.cpp:127`). `FieldVarNode::translate2`
  (`tree/var/field_var.cpp:46`) fa `CreateGEP` su
  `expr->sem_type->structType()->structtype`, che per una classe dichiarata più
  avanti è ancora un `StructType::create` opaco (`tree/type.cpp:178-182`).
- Scenario: `Class A … Method M%() Local o.B = … : Return o\x … ` con `Class B`
  dichiarata dopo `A` (il semant passa perché i tipi sono tutti protoati) →
  atteso assert/crash di LLVM su GEP in tipo non dimensionato. Le Function non
  sono affette (tradotte dopo tutte le struct).
- Correzione suggerita: tradurre i metodi in una passata separata dopo
  `structs->translate2` (specularmente alla correzione di C2).

---

## Minori

### M1. Nessun null-check sulle chiamate a metodo, nemmeno in debug
`MethodCallNode::translate/translate2/translate3` non controllano mai l'oggetto
(`FieldVarNode::translate` in debug emette `__bbNullObjEx`, riga 35-36).
`Local p.Player = Null : p\M()` crasha senza l'errore runtime Blitz. Suggerito:
in debug, stesso check dei field var sull'oggetto prima della chiamata.

### M2. `Static Method constructor/destructor` accettati ma chiamati con `self`
`NewNode::semant` (`tree/expr/new.cpp:35-41`) e `DeleteNode` passano sempre
l'oggetto come primo argomento e saltano il primo parametro nel cast: se il
costruttore/distruttore è `Static` le firme si sfalsano (argomenti shiftati o
"Too many parameters"). Suggerito: errore esplicito in parse/semant.

### M3. Costruttore non ereditato
`New Figlio(args)` con costruttore definito solo nel padre → "Constructor not
found in class 'figlio'" (`new.cpp:17-24`). Coerente col non-dispatch ma
sorprendente rispetto ai metodi che risalgono la catena: da documentare o
implementare la risalita.

### M4. Nessun dispatch dinamico (override solo sul tipo statico)
`MethodCallNode::semant` risolve sul tipo statico dell'espressione: con
`Local a.A = <oggetto B>`, `a\M()` chiama `A_M` anche se `B` ridefinisce `M`.
È una scelta di design (niente vtable), ma va documentata in CHANGES/CLAUDE.md
perché chi viene da altri linguaggi OOP si aspetta l'opposto.

### M5. Comandi `system()` con path interpolati senza quoting robusto
`linker_gcc/linker_gcc.cpp` (createExe/compileToObject),
`codegen_c/codegen_c.cpp:751`, `package/package_apk.cpp` (RUN con `find/cp` e
`bundle.sourceDir` non quotato, riga ~130): path con spazi o caratteri shell
rompono la build (iniezione teorica). Suggerito: quoting sistematico o
`exec*`/`std::filesystem`.

### M6. Backend C su macOS: `-Wl,--start-group` non esiste su ld64
`linker_gcc/linker_gcc.cpp:203`: il linker Apple non supporta
`--start-group/--end-group` (e `-s` è deprecato): il percorso "clang su mac"
previsto dallo stesso file non può linkare. Suggerito: su Apple omettere il
gruppo (ld64 risolve i cicli da sé).

### M7. Letterali interi a 32 bit in un linguaggio a 64 bit
`parser.cpp:1115` usa `atoi` e `IntConstNode.value` è `int`
(`tree/expr/int_const.h:7`), ma `bb_int_t` è `int64_t` nei backend LLVM/C:
`Local x = 5000000000` tronca silenziosamente (ereditato dal fork, ma ora
visibile; anche `Codegen_C::constantInt(int)`). Suggerito: `strtoll` + campo a
64 bit.

### M8. package_apk: targetSdk incoerente e auto-bundle indiscriminato
`package/package_apk.cpp`: manifest con `targetSdkVersion="33"` hardcoded
mentre si compila contro `android-34/android.jar` hardcoded (e la doc del
progetto dichiara targetSdk 34); l'auto-bundle copia **tutti** i file della
cartella sorgente (profondità 10) negli asset dell'APK, inclusi file estranei.
Suggerito: allineare le versioni e filtrare/esplicitare gli asset.

### M9. Leak su percorsi d'errore del parser
Es. `parser.cpp:262` (`ex("Unexpected expression")` perde `expr_or_var`),
pattern analoghi nei rami OOP nuovi. Tollerabile (il processo termina), ma gli
`a_ptr` usati nel resto del file coprirebbero anche questi rami.

### M10. Dettagli backend C
`tree/stmt/delete_each.cpp:24`: usa `g->emit` senza newline e costruisce il
nome tipo a mano (`"_tbb_"+ident` invece di `"_t"+toCSafeName(ident)` —
equivalente oggi, fragile se cambia `toCSafeName`). `Field x = 100` dentro
`Class`/`Type` è parsato ma l'inizializzatore non viene mai applicato da `New`
su LLVM/C (ereditato per i Type; con le classi diventa più visibile: meglio un
errore o l'applicazione nel `New`). Le funzioni userlib (`.decls`) non sono
supportate dal backend C e falliscono solo al link con un undefined symbol.

---

## Nota sui punti verificati e risultati OK

- `StructType::canCastTo` con risalita della catena (`type.cpp:155-163`) e
  layout dei campi ereditati (prefisso identico padre/figlio, offset `k*4`)
  sono corretti e coerenti tra backend.
- Il lookup dei metodi con risalita (`method_call.cpp:21-30`) e la gestione
  `%/#/$` dei tag di ritorno via `findType` sono corretti.
- `NewNode::translate` x86 usa correttamente un temp per evitare la doppia
  emissione di `__bbObjNew` (ma resta vittima di C1).
- Le correzioni a `main.cpp` (parsing opzioni `-r/-target/-sign/-team`),
  `libs.cpp` (snprintf, realpath con fallback), `linker_lld.cpp` (API level
  Android, path clang 17, xcrun con fallback) e `codegen_llvm.cpp`
  (`-outline-atomics` su Android arm64) sono corrette alla lettura.
