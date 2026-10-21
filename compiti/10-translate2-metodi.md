# Compito 10: metodi LLVM tradotti dopo i corpi di tutte le classi (finding I9)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## Obiettivo

Nel backend LLVM di `blitzcc` i corpi dei `Method` vengono tradotti dentro
`ClassDeclNode::translate2`, cioè mentre `structs->translate2` imposta il body
llvm delle classi una per una. Un Method della classe A che legge o scrive un
campo di una classe (o di un `Type`) dichiarata DOPO A genera un
`CreateGEP` su una `llvm::StructType` ancora opaca. Va spostata la traduzione
dei metodi in una passata dedicata `translate2Methods`, chiamata da
`ProgNode::translate2` subito dopo `structs->translate2`, sul modello già
collaudato di `protoMethods` (commit 0bac047) e `semantMethods` (commit 02d4f85).

**Decisione già presa dall'utente**: correzione strutturale senza test
runtime LLVM. Sulla macchina locale il toolchain LLVM non c'è
(`USE_LLVM:BOOL=OFF`): tutto il codice sotto `#ifdef USE_LLVM` NON viene
compilato dalla build locale. Un errore di battitura lì dentro passerebbe
inosservato: vedi "Prudenza sul codice LLVM".

Vincoli assoluti:
- Nessuna modifica fuori da `/home/rufio72/blitx3d_linux`; dentro, solo i file
  elencati in "Struttura da realizzare". Non toccare file già modificati o
  non tracciati presenti prima di iniziare (fotografa `git status --short`
  all'inizio e confrontalo alla fine).
- Commenti e documenti in italiano; i messaggi `Expect` del test restano in
  inglese come il resto di `test/language/`.
- Nessun file di codice sopra le 800 righe (oggi nessun `.cpp`/`.h` del
  compilatore li supera; i file toccati sono tutti sotto le 450 righe).
- Non cambiare: `translate` (x86), `translate3` (backend C), `proto`,
  `protoMethods`, `semant`, `semantMethods`, il resto di
  `ClassDeclNode::translate2` (body della struct, template del tipo).
- Niente installazioni, niente push. `blitzcc` vuole `-o` PRIMA del sorgente
  e la variabile `blitzpath`.

## Verdetto sul finding (verificato leggendo il codice)

Il difetto è **confermato** dalla lettura. Percorsi relativi a
`src/tools/compiler/tree/`.

1. `prognode.cpp:249` `structs->translate2( g );`, poi r.251
   `funcs->translate2( g );`, r.253 `g->builder->SetInsertPoint( block );`,
   r.258 `stmts->translate2( g );`, r.266 `g->verify();`. La funzione
   `ProgNode::translate2` sta tra r.185 `#ifdef USE_LLVM` e r.268 `#endif`.
2. `decl/class_decl.cpp:106-189` (`#ifdef USE_LLVM` … `#endif`):
   `ClassDeclNode::translate2` costruisce i campi, r.133-134 chiama
   `sem_type->llvmTypeDef( g )` e `sem_type->llvmType(...)`, r.139
   `ty->setBody( fields_vec );` (body della SUA struct), r.144
   `ty2->setBody( templatefields );`, e infine r.186-187:
   ```cpp
   	// Translate methods
   	methods->translate2( g );
   ```
   Quindi i metodi della classe k sono tradotti quando le classi k+1, k+2, …
   non hanno ancora il body.
3. `type.cpp:178-183` `StructType::llvmType`: se `structtype` è nullo crea
   `llvm::StructType::create( *c,"_t"+ident )`, cioè una struct **opaca**;
   l'unico `setBody` è in `ClassDeclNode::translate2` (r.139) e in
   `StructDeclNode::translate2` (`decl/struct_decl.cpp:90`) (grep
   `setBody`/`structtype`).
4. `var/field_var.cpp:41-47` `FieldVarNode::translate2`: indici `{0,
   offset/4+1}` e `g->builder->CreateGEP( expr->sem_type->structType()->structtype,
   … )`. Se la classe del campo è dichiarata dopo, `structtype` è opaca
   (creata da `llvmType` per la `Local o.B` o per il parametro `b.B`).
   In LLVM 19 il costruttore di `GetElementPtrInst` calcola
   `ResultElementType=getIndexedType(...)`: su struct opaca l'indice 1 non è
   valido e il risultato è nullo (con LLVM compilato con le asserzioni ci si
   aspetta un assert; senza, un'istruzione con tipo risultato nullo che può
   far cadere la pipeline O3 di `Codegen_LLVM::optimize`,
   `codegen_llvm/codegen_llvm.cpp:201-229`, chiamata da `main.cpp:568`).
   Il sintomo esatto è «non verificabile in locale»; l'ordine sbagliato no.
5. Le `Function` non sono affette: `funcs->translate2` gira dopo
   `structs->translate2`. Né lo è l'accesso ai campi della propria classe o
   delle superclassi (`Extends` richiede la superclasse già dichiarata, e il
   body proprio è impostato a r.139, prima dei metodi).
6. Backend C (`translate3`): **non affetto**. `ProgNode::translate3`
   (`prognode.cpp:278` e seguenti) dichiara in anticipo tutte le struct
   (ciclo su `sem_env->types`, r.282-295), e `FieldVarNode::translate3`
   (`var/field_var.cpp:53-70`) accede ai campi come
   `((BBObj*)p)->fields[i].INT/.FLT/.STR/.OBJ`, senza dipendere dal layout
   della struct. Misurato oggi: lo scenario I9 (verifica 3) compila e dà
   `Pass: 2. Fail: 0` col backend C. Nessuna modifica a `translate3`.
7. Backend x86 (`translate`): OOP già rifiutato in `ProgNode::translate`
   (commit b3c4aa0). Nessuna modifica.
8. `DeclNode::translate2` non è vuota: `decl/node.cpp:4-5` stampa
   `translate2 missing implementation for …`. La nuova virtuale deve quindi
   avere il corpo vuoto `{}` inline in `node.h` (altrimenti ogni `Type`,
   `Const`, ecc. nella sequenza `structs` stamperebbe l'avviso o non
   linkerebbe).
9. `FuncType::llvmFunction` (`type.cpp:113` e seguenti) cerca la funzione per
   nome con `g->module->getFunction( sym )`: l'ordine di traduzione dei
   corpi non cambia i simboli. Spostare i metodi dopo i body delle struct ma
   prima di `funcs->translate2` lascia invariato l'ordine relativo
   metodi/funzioni nel modulo.

## Cosa esiste già (fatti verificati)

| File | Righe | Ruolo |
|---|---|---|
| `decl/node.h` | 26 | `DeclNode`: `protoMethods` r.11 e `semantMethods` r.13 (vuote); blocco `#ifdef USE_LLVM` r.16-19 con `translate2` r.17 e `transdata2` r.18 |
| `decl/decl_seq.h` | 31 | `DeclSeqNode`: `protoMethods` r.11, `semantMethods` r.13; blocco `#ifdef USE_LLVM` r.21-24 (`translate2` r.22, `transdata2` r.23) |
| `decl/decl_seq.cpp` | 128 | `protoMethods` r.17-26 (modello del ciclo try/catch); blocco `#ifdef USE_LLVM` r.72-94: `translate2` r.73-82, `transdata2` r.84-93, `#endif` r.94 |
| `decl/class_decl.h` | 31 | `ClassDeclNode`: `protoMethods` r.17, `semantMethods` r.19; blocco `#ifdef USE_LLVM` r.21-23 (`translate2` r.22) |
| `decl/class_decl.cpp` | 210 | `protoMethods` r.24-29 (commento italiano di 3 righe + funzione: modello di stile), `translate2` r.106-188, `#endif` r.189 |
| `prognode.cpp` | 435 | `ProgNode::translate2` r.189-267 |

- Build: in `src/tools/compiler/CMakeLists.txt` r.5 `option(USE_LLVM … OFF)`;
  r.7-35 cerca LLVM 19 solo se `USE_LLVM`; r.196
  `target_compile_definitions(blitzcc PRIVATE USE_LLVM)` e r.198-213
  `codegen_llvm/*` solo dentro `if(LLVM_FOUND)`. `build/x86_64-linux-release/CMakeCache.txt`:
  r.1303 `USE_GCC_BACKEND:BOOL=ON`, r.1306 `USE_LLVM:BOOL=OFF`.
  I file `tree/**` sono compilati sempre, ma le parti sotto `#ifdef USE_LLVM`
  vengono scartate dal preprocessore.
- Nessun controllo di compilazione alternativo disponibile: sulla macchina non
  ci sono header LLVM (`/usr/lib/llvm-18` ha solo `lib/`, nessun
  `IRBuilder.h` nel filesystem, `deps/llvm` contiene solo script di build),
  e la CI (`.github/workflows/ci.yml`) parte solo su `master` e non attiva
  `USE_LLVM`. Da qui la prudenza sotto.
- Baseline misurata oggi (compilatore di `b98114e`): suite `Pass: 491. Fail: 17`
  (i 17 sono ambientali, preesistenti); scenario I9 col backend C
  `Pass: 2. Fail: 0`.

## Prudenza sul codice LLVM

Tutte le righe nuove dentro `#ifdef USE_LLVM` vanno scritte copiando
**letteralmente** il testo prescritto qui sotto (che ricalca righe esistenti e
compilate in passato nella stessa unità: `DeclSeqNode::translate2`,
`Codegen_LLVM *g`). Niente riformattazioni, niente nomi diversi, niente
include nuovi. Dopo le modifiche, rileggi ogni hunk sotto `USE_LLVM` carattere
per carattere contro questo brief (verifica 6) e controlla l'equilibrio
`#if`/`#endif` (verifica 7). Le firme devono coincidere esattamente tra
`node.h`, `decl_seq.h/.cpp`, `class_decl.h/.cpp` (`void translate2Methods(
Codegen_LLVM *g )`), altrimenti l'override non scatta e il bug resta (nessun
errore di compilazione lo segnalerebbe in un build LLVM, salvo `override`,
che il progetto non usa: non aggiungerlo).

## Struttura da realizzare

1. `decl/node.h`: nel blocco `#ifdef USE_LLVM`, subito dopo la riga
   `	virtual void translate2( Codegen_LLVM *g );` (r.17) aggiungere:
   ```cpp
   	virtual void translate2Methods( Codegen_LLVM *g ){}
   ```
2. `decl/decl_seq.h`: nel blocco `#ifdef USE_LLVM`, dopo
   `	void translate2( Codegen_LLVM *g );` (r.22) aggiungere
   `	void translate2Methods( Codegen_LLVM *g );`.
   `decl/decl_seq.cpp`: dentro il blocco `#ifdef USE_LLVM`, subito dopo la
   chiusura di `DeclSeqNode::translate2` (r.82) e prima di `transdata2`,
   aggiungere (riga vuota prima):
   ```cpp
   void DeclSeqNode::translate2Methods( Codegen_LLVM *g ){
   	for( int k=0;k<decls.size();++k ){
   		try{ decls[k]->translate2Methods( g ); }
   		catch( Ex &x ){
   			if( x.pos<0 ) x.pos=decls[k]->pos;
   			if(!x.file.size() ) x.file=decls[k]->file;
   			throw;
   		}
   	}
   }
   ```
3. `decl/class_decl.h`: nel blocco `#ifdef USE_LLVM`, dopo
   `	void translate2( Codegen_LLVM *g );` (r.22) aggiungere
   `	void translate2Methods( Codegen_LLVM *g );`.
   `decl/class_decl.cpp`: in `ClassDeclNode::translate2` togliere le righe
   185-187 (la riga vuota prima del commento, `	// Translate methods` e
   `	methods->translate2( g );`), così la funzione termina con
   `	temp->setInitializer( init );` seguito da `}`. Poi, dopo la `}` di
   `translate2` e PRIMA dell'`#endif` di r.189, aggiungere (riga vuota prima):
   ```cpp
   // Corpi dei metodi: eseguito da ProgNode::translate2 dopo
   // structs->translate2, così i campi di classi e Type dichiarati più avanti
   // hanno già il body llvm (niente GEP su struct opache).
   void ClassDeclNode::translate2Methods( Codegen_LLVM *g ){
   	methods->translate2( g );
   }
   ```
4. `prognode.cpp`: tra `	structs->translate2( g );` (r.249) e
   `	funcs->translate2( g );` (r.251) inserire
   `	structs->translate2Methods( g );` così:
   ```cpp
   	structs->translate2( g );
   	structs->translate2Methods( g );

   	funcs->translate2( g );
   ```
5. Test nuovo `test/language/class_forward_fields.bb` (vedi domanda aperta 2,
   proposta accettata salvo diversa decisione), con esattamente:
   ```blitz3d
   ; Method che accedono ai campi di una Class dichiarata dopo (finding I9)

   Class ClassFwdFA
   	Field v

   	Method ReadB(b.ClassFwdFB)
   		Return b\w + Self\v
   	End Method

   	Method MakeB.ClassFwdFB(n)
   		Local o.ClassFwdFB = New ClassFwdFB()
   		o\w = n
   		Return o
   	End Method
   End Class

   Class ClassFwdFB
   	Field w
   	Field name$
   End Class

   Local classFwdFA.ClassFwdFA = New ClassFwdFA()
   classFwdFA\v = 1
   Local classFwdFB.ClassFwdFB = classFwdFA\MakeB(41)

   Expect classFwdFB\w = 41, "a Method writes a field of a Class declared after"
   Expect classFwdFA\ReadB(classFwdFB) = 42, "a Method reads a field of a Class declared after"
   ```
   `test/language.bb`: aggiungere `Include "language/class_forward_fields.bb"`
   subito dopo `Include "language/class_forward.bb"` (r.5).

Nessun'altra modifica. Nomi e posizioni sopra sono prescritti.

## Verifiche obbligatorie

Nei comandi: `R=/home/rufio72/blitx3d_linux`, `T=$(mktemp -d)`,
`export blitzpath=$R/_release`, `B=$R/_release/bin/blitzcc`.

1. Compilazione: `ninja -C $R/build/x86_64-linux-release blitzcc` → termina
   senza errori, `$B` ha data di modifica nuova. (Compila solo il percorso
   non-LLVM: `node.h`, `decl_seq.h`, `class_decl.h` vengono comunque
   preprocessati, quindi un `#endif` perso o un blocco rotto fuori da
   `USE_LLVM` verrebbe segnalato; un errore DENTRO `USE_LLVM` no.)
2. Percorso LLVM: compilazione e scenario I9 col backend LLVM
   → «non verificabile in locale» (niente toolchain LLVM 19, `-llvm` risponde
   `not compiled with llvm support`). Non tentare installazioni.
3. Scenario I9 col backend C, file `$T/i9.bb` con lo stesso contenuto del test
   del punto 5 (o il test stesso):
   `cd $T && $B -q -r test -o $T/cff $R/test/language/class_forward_fields.bb && ./cff; echo rc=$?`
   → 2 righe `PASS:` e `Pass: 2. Fail: 0`, `rc=0`. Misurato oggi col
   compilatore attuale (stesso programma con nomi `I9A`/`I9B`): `Pass: 2.
   Fail: 0`, deve restare così dopo.
4. Non regressione sample OOP, eseguiti in `$T`:
   ```
   cd $T; for f in test_super test_inheritance test_multi_inheritance; do
     $B -q -r test -o $T/$f $R/_release/samples/oop/$f.bb && ./$f | tail -1; done
   for f in test_super test_inheritance test_multi; do
     diff $T/${f}_result.txt $R/_release/samples/oop/${f}_result.txt && echo "$f identico"; done
   ```
   → tre `Pass: 0. Fail: 0` e `test_super identico`, `test_inheritance identico`,
   `test_multi identico`.
5. Suite completa:
   `cd $R/test && $B -q -r test -o $T/all_test all.bb && DISPLAY=:1 timeout 280 $T/all_test | tail -2`
   → `Pass: 493. Fail: 17` con il test nuovo (baseline misurata oggi
   `Pass: 491. Fail: 17` + 2 Expect); `Pass: 491. Fail: 17` se la domanda 2
   viene respinta. Gli avvisi gcc e le righe `GL Version`/`hello, stderr` sono
   preesistenti.
6. Revisione del testo LLVM:
   `git -C $R diff src/tools/compiler/tree/` → esattamente gli hunk dei punti
   1-4: in `node.h`, `decl_seq.h`, `class_decl.h` una riga aggiunta ciascuno;
   in `decl_seq.cpp` la funzione `DeclSeqNode::translate2Methods` (11 righe
   con la vuota); in `class_decl.cpp` 3 righe tolte (vuota, commento,
   `methods->translate2( g );`) e la funzione nuova con commento; in
   `prognode.cpp` una riga. Ogni riga aggiunta deve coincidere col brief
   (tab compresi, `Codegen_LLVM *g` con lo spazio). Riportare il diff integrale
   nel rapporto.
7. Equilibrio preprocessore, invariato rispetto a oggi:
   ```
   cd $R/src/tools/compiler/tree; for f in decl/node.h decl/decl_seq.h decl/decl_seq.cpp decl/class_decl.h decl/class_decl.cpp prognode.cpp; do echo "$f $(grep -c '^#if' $f) $(grep -c '^#endif' $f)"; done
   ```
   → `decl/node.h 3 3`, `decl/decl_seq.h 3 3`, `decl/decl_seq.cpp 2 2`,
   `decl/class_decl.h 3 3`, `decl/class_decl.cpp 2 2`, `prognode.cpp 2 2`
   (misurato oggi). Inoltre `grep -n "translate2Methods" -r $R/src/tools/compiler/tree`
   → 7 righe: `node.h`, `decl_seq.h`, `decl_seq.cpp` (2: definizione e
   chiamata nel ciclo), `class_decl.h`, `class_decl.cpp` (1: definizione),
   `prognode.cpp`; e ognuna di esse deve stare tra un `#ifdef USE_LLVM` e il
   suo `#endif` (controllare a occhio con i numeri di riga).
   `grep -n "methods->translate2" $R/src/tools/compiler/tree/decl/class_decl.cpp`
   → una sola riga, dentro `translate2Methods`.
8. Ambito: `git -C $R status --short` → rispetto alla fotografia iniziale
   compaiono solo i 6 file di `src/tools/compiler/tree/` (`prognode.cpp`,
   `decl/node.h`, `decl/decl_seq.h`, `decl/decl_seq.cpp`, `decl/class_decl.h`,
   `decl/class_decl.cpp`), `M test/language.bb`,
   `?? test/language/class_forward_fields.bb`. Nessun `*_result.txt` scritto
   in `_release/samples/oop/`.
9. Soglia righe:
   `find $R/src/tools/compiler \( -name '*.cpp' -o -name '*.h' \) | xargs wc -l | awk '$1>800 && $2!="totale"'`
   → nessuna riga (misurato oggi).

## Consegna

- Un commit con i soli file del fix (e del test), messaggio in inglese:
  ```
  Compiler: translate LLVM method bodies after all class bodies (I9)

  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
  Nessun push.
- Rapporto finale di massimo trenta righe: uscita delle verifiche 1-9 (righe
  `Pass/Fail`, diff integrale del punto 6, conteggi del punto 7,
  `git status`), eventuali deviazioni dal brief.

## Domande aperte

1. Nome della passata: `translate2Methods` (suffisso come `protoMethods`/
   `semantMethods`, prefisso `translate2` per dire a quale backend
   appartiene). Alternativa: `translateMethods2`. Proposta:
   `translate2Methods`. «preferenza»
2. Aggiungere il test `test/language/class_forward_fields.bb` anche se il
   backend C passa già oggi? Proposta: sì, documenta il caso e diventa la
   regressione per il giorno in cui la suite girerà con LLVM; suite a
   493/17. «preferenza»
3. Posizione della chiamata: subito dopo `structs->translate2` e prima di
   `funcs->translate2` (ordine metodi/funzioni nel modulo invariato).
   Alternativa: dopo `funcs->translate2`. Proposta: prima. «preferenza»
4. Commit di una correzione che non si può compilare né eseguire nel percorso
   toccato: accettato dall'utente («fix strutturale senza test runtime
   LLVM»); si suggerisce di annotare in `CODE_REVIEW_COMPILER.md` (lo fa la
   chat principale, fuori dal commit) che I9 è corretto ma «non verificato su
   LLVM». «scelta di progetto», già decisa.
