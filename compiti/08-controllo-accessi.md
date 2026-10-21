# Compito 08: controllo accessi Private/Protected (finding I1)

Data: 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## 1. Contesto e obiettivo

Il compilatore `blitzcc` (estensione OOP di Blitz3D: `Class`/`Method`/`Self`/
`Super`/`Static`, metodi compilati come funzioni globali
`ClassName_MethodName(self,...)`) ha il controllo accessi `Private`/`Protected`
aggirabile e incoerente (finding I1 di CODE_REVIEW_COMPILER.md, branch
`origin/compiler-code-review`): il contesto di classe è dedotto dall'esistenza
di un PARAMETRO chiamato `self`, e i check confrontano col tipo statico
dell'espressione invece che con la classe proprietaria del membro. Obiettivo:
un vero contesto di classe in `Environ` impostato dal semant dei metodi, check
basati su `ownerClass`, più i test che fissano la semantica.

### Vincoli assoluti

- Nessuna modifica fuori dal repository `/home/rufio72/blitx3d_linux`.
- Non toccare `tree/parser.cpp`, `tree/toker.cpp`, i codegen
  (`codegen_llvm/`, `codegen_c/`, `codegen_x86/`) né il layout/offset dei campi.
- Semantica dei `Type` classici (struct_decl) invariata: non hanno modificatori
  d'accesso, i check scattano solo sui flag `DECL_PRIVATE`/`DECL_PROTECTED`.
- Nessun file di codice sopra le 800 righe (i file toccati sono tutti sotto le
  300, verificato: environ.cpp 77, field_var.cpp 125, class_decl.cpp 201,
  method_call.cpp 294, func_decl.cpp 169).
- Commenti nel codice in inglese (coerenti coi file esistenti); questo brief e
  il rapporto in italiano.
- Nessun push.

## 2. Cosa esiste già (fatti verificati, da lettura diretta dei file)

| File (da src/tools/compiler/) | Righe | Ruolo |
|---|---|---|
| `environ.h` | 49 | classe `Environ` (stack frame); dichiara `getCurrentClass()` a riga 46; include già `tree/type.h` (riga 15), quindi `StructType` è visibile |
| `environ.cpp` | 77 | `getCurrentClass()` a 69-77: euristica sul parametro `self` |
| `tree/decl.h` | 64 | flag a 12-16 (`DECL_FIELD=64`, `DECL_PRIVATE=128`, `DECL_PUBLIC=256`, `DECL_PROTECTED=512`); `Decl::ownerClass` a riga 28, inizializzato a 0 nel costruttore (riga 29) |
| `tree/var/field_var.cpp` | 125 | `FieldVarNode::semant` 6-31: check Private a 14-19 (`currentClass != s`, col tipo statico `s` dell'espressione), check Protected a 23-28 (`currentClass->isSubclassOf( s )`) |
| `tree/expr/method_call.cpp` | 294 | `MethodCallNode::semant` 7-73: check Private a 37-43 (`currentClass != sem_decl->ownerClass`, già corretto), Protected a 47-52 (`isSubclassOf( sem_decl->ownerClass )`, già corretto). `SuperMethodCallNode::semant` 164-208: NESSUN check di accesso; risolve `sem_decl` risalendo i padri (178-182), trova `self` con `e->findDecl("self")` (188-189) |
| `tree/decl/class_decl.cpp` | 201 | `ClassDeclNode::semant` 27-48: copia i campi ereditati dal padre (30-39, `insertDecl` a 35 — `ownerClass` NON viene impostato), poi `fields->proto` (42) per i campi propri (`ownerClass` resta 0). `semantMethods` 52-54 chiama `methods->semant(e)` |
| `tree/decl/func_decl.cpp` | 169 | `proto` 6-24: imposta `decl->ownerClass` per i metodi (16-22, via `e->findType(ownerClassName)`). `semant` 26-38: crea `sem_env=d_new Environ(genLabel(),sem_type->returnType,1,e)` a riga 28 e inserisce i parametri; non sa nulla della classe |
| `tree/decl/func_decl.h` | — | `FuncDeclNode::ownerClassName` (riga 14, stringa vuota per le Function normali; impostata sia per i metodi d'istanza sia per gli Static, vedi parser.cpp 842-886) |
| `tree/prognode.cpp` | 434 | ordine del semant (8-28): `structs->proto`, `structs->semant`, `funcs->proto`, `stmts->semant`, `structs->semantMethods` (riga 21), `funcs->semant` |
| `tree/type.h` | — | `StructType::isSubclassOf(StructType*)` a 103-110: risale `superType`, vero anche per la classe stessa |
| `tree/expr/call.cpp` | — | `CallNode::semant` 82+: `findFunc` a riga 84, NESSUN check di accesso (le chiamate col nome manglato `ClassName_MethodName(...)` bypassano tutto; sono anche l'unica sintassi per chiamare gli Static Method, vedi `test/language/class_scope.bb:42`: `ClassScopeGame_Count()`) |

Altri fatti:
- `parser.cpp:857`: il parametro implicito `self` è aggiunto solo ai metodi non
  statici; `parser.cpp:799-833`: il modificatore è un prefisso della singola
  dichiarazione (`Private Field x`, `Protected Method M()`, anche
  `Private Static Method ...`); il costruttore è un metodo normale
  (`ClassName_constructor`), quindi passa da `parseMethodDecl` con
  `ownerClassName` impostato.
- Una classe `Extends` richiede il padre dichiarato prima (il `proto` risolve
  `superName` con `findType` in ordine di dichiarazione), e `structs->semant`
  gira nello stesso ordine: quando il figlio copia i campi, il padre ha già i
  propri `ownerClass` impostati (dopo questa modifica).
- Finding I2 (non in questo compito): le firme dei metodi NON possono
  riferire classi dichiarate dopo; nei test evita parametri di metodo tipati
  con una classe successiva (sotto si usa una `Global` tipata, vista dai corpi
  dei metodi grazie al commit 02d4f85: `semantMethods` gira dopo
  `stmts->semant`).
- Gli unici `d_new Environ` nel semant sono prognode.cpp:14 (programma),
  func_decl.cpp:28 (per funzione/metodo) e libs.cpp:319 (runtime): non esistono
  environ annidati dentro un corpo di metodo.
- `Expect` è il modulo unit-test runtime: ogni `Expect` conta 1 in
  `Pass:`/`Fail:` (src/modules/bb/unit-test/unit-test.cpp:170).
- I test negativi (che devono NON compilare) stanno per convenzione in
  `test/syntax-errors/` (es. `arithmetic-on-type.bb`); quelli positivi in
  `test/language/*.bb`, inclusi da `test/language.bb` (in ordine alfabetico).
- Samples OOP in `_release/samples/oop/`: `test_super.bb`,
  `test_inheritance.bb`, `test_multi_inheritance.bb` con i rispettivi
  `*_result.txt`; non usano `Private`/`Protected` (grep), quindi devono restare
  invariati e funzionanti.

## 3. Semantica bersaglio

1. Contesto di classe: il `sem_env` di un metodo (d'istanza O statico) di
   classe `C` ha contesto `C`; ogni altro environ (programma, Function normali,
   anche con un parametro chiamato `self`) non ha contesto.
2. `Private` (campo o metodo): accesso consentito solo se
   `getCurrentClass() == ownerClass` del membro. Il tipo statico
   dell'espressione non conta più.
3. `Protected`: consentito solo se `getCurrentClass() != 0` e
   `getCurrentClass()->isSubclassOf( ownerClass )`.
4. `Public` o nessun modificatore: nessun check (come oggi).
5. Campo `Private` ereditato: il campo copiato nel figlio conserva
   l'`ownerClass` della classe che lo ha dichiarato, quindi la sottoclasse NON
   vi accede (vedi domanda aperta Q1).
6. `Super\Metodo()`: stessi check 2-3 sul metodo risolto (un metodo `Private`
   del padre via `Super` è errore; `Protected` è sempre consentito perché il
   chiamante è sottoclasse). Vedi Q3.
7. `Static Method`: ha il contesto della propria classe, quindi pieno accesso
   ai membri privati propri attraverso un oggetto (vedi Q4).

## 4. Modifiche richieste, file per file

Tutti i percorsi sotto `src/tools/compiler/` salvo i test.

1. `environ.h`: aggiungi il membro `StructType *currentClass;` (per esempio
   dopo `returnType`) con commento in inglese
   `//class whose method body this environ belongs to (access control)`.
2. `environ.cpp`: inizializza `currentClass(0)` nel costruttore (riga 5-6);
   riscrivi `getCurrentClass()` (69-77) eliminando l'euristica su `self`:
   ```cpp
   StructType *Environ::getCurrentClass(){
       for( Environ *e=this;e;e=e->globals ){
           if( e->currentClass ) return e->currentClass;
       }
       return 0;
   }
   ```
3. `tree/decl/func_decl.cpp`, `FuncDeclNode::semant` (26-38): subito dopo la
   creazione di `sem_env` (riga 28), se `ownerClassName` non è vuota imposta
   `sem_env->currentClass = e->findType( ownerClassName )->structType()`
   (con guardia sul `findType` nullo, come già fa `proto` a 17-22). Vale anche
   per gli Static Method e per `_constructor`.
4. `tree/decl/class_decl.cpp`, `ClassDeclNode::semant` (27-48):
   - nel ciclo di copia dei campi ereditati (32-38), dopo `d->offset=k*4`
     aggiungi `d->ownerClass=parentDecl->ownerClass;` (conserva il vero
     proprietario anche su catene a più livelli);
   - dopo `fields->proto( sem_type->fields,e )` (riga 42), nel ciclo dei campi
     propri (45-47) imposta anche
     `sem_type->fields->decls[k]->ownerClass=sem_type;`.
5. `tree/var/field_var.cpp`, `FieldVarNode::semant` (14-28): sostituisci i due
   confronti col tipo statico `s`:
   - Private: `if( !currentClass || currentClass != sem_field->ownerClass )`;
   - Protected: `if( !currentClass || !currentClass->isSubclassOf( sem_field->ownerClass ) )`.
   Messaggi d'errore invariati (`Cannot access private field '...'`,
   `Cannot access protected field '...'`).
6. `tree/expr/method_call.cpp`, `SuperMethodCallNode::semant`: dopo la
   risoluzione di `sem_decl` (riga 183-185), aggiungi gli stessi due check di
   `MethodCallNode::semant` (37-52), con messaggi
   `Cannot access private method '...'` / `Cannot access protected method '...'`.
   I check di `MethodCallNode` restano come sono (già su `ownerClass`).
7. Solo se Q5 è accettata — `tree/expr/call.cpp`, `CallNode::semant` (dopo la
   riga 85): se `sem_decl->ownerClass` è impostato, applica gli stessi check
   Private/Protected con `e->getCurrentClass()`.
8. Test positivi: nuovo `test/language/access.bb` (contenuto sotto, 5
   `Expect`) e in `test/language.bb` aggiungi
   `Include "language/access.bb"` come prima riga (ordine alfabetico).
9. Test negativi: quattro nuovi file in `test/syntax-errors/` (contenuto
   sotto). Non vanno inclusi in `language.bb`.

### Contenuto di `test/language/access.bb`

```blitz3d
; Controllo accessi Private/Protected (finding I1)

Class AccVault
	Private Field code
	Protected Field alarm

	Method Fill()
		Self\code = 42
		Self\alarm = 7
	End Method

	Method GetCode()
		Return Self\code
	End Method

	Method PeekSub()
		; espressione con tipo statico della sottoclasse, contesto = proprietario
		Return accSubGlobal\code + accSubGlobal\alarm
	End Method

	Static Method Unlock(v.AccVault)
		Return v\code
	End Method
End Class

Class AccSafe Extends AccVault
	Method GetAlarm()
		Return Self\alarm
	End Method
End Class

Class AccBase
	Protected Method Secret()
		Return 5
	End Method
End Class

Class AccDer Extends AccBase
	Method Reveal()
		Return Super\Secret() + 1
	End Method
End Class

Global accSubGlobal.AccSafe = New AccSafe()
accSubGlobal\Fill()

Local accV.AccVault = New AccVault()
accV\Fill()
Local accD.AccDer = New AccDer()

Expect accV\GetCode() = 42, "a Method reads a Private field of its own class"
Expect AccVault_Unlock(accV) = 42, "a Static Method reads a Private field of its own class"
Expect accSubGlobal\GetAlarm() = 7, "a subclass Method reads an inherited Protected field"
Expect accV\PeekSub() = 49, "an owner Method accesses members through a subclass-typed expression"
Expect accD\Reveal() = 6, "Super calls a Protected parent method"
```

### Test negativi (`test/syntax-errors/`)

`access-private-function-self.bb` (Function con parametro `self`: l'exploit I1):
```blitz3d
Class Cassaforte
	Private Field codiceSegreto
End Class

Function Ruba%(self.Cassaforte)
	Return self\codiceSegreto
End Function

Local c.Cassaforte = New Cassaforte()
Print Ruba(c)
```

`access-private-inherited.bb` (Private ereditato, vedi Q1):
```blitz3d
Class AccIBase
	Private Field segreto
End Class

Class AccIFiglia Extends AccIBase
	Method Leggi()
		Return Self\segreto
	End Method
End Class

Local f.AccIFiglia = New AccIFiglia()
Print f\Leggi()
```

`access-super-private.bb` (Super su metodo Private, vedi Q3):
```blitz3d
Class AccSPadre
	Private Method Nascosto()
		Return 1
	End Method
End Class

Class AccSFiglio Extends AccSPadre
	Method Prova()
		Return Super\Nascosto()
	End Method
End Class

Local x.AccSFiglio = New AccSFiglio()
Print x\Prova()
```

`access-private-outside.bb` (regressione: già errore oggi, deve restarlo):
```blitz3d
Class AccScrigno
	Private Field perla
End Class

Local s.AccScrigno = New AccScrigno()
Print s\perla
```

## 5. Verifiche obbligatorie

Ambiente: `export blitzpath=/home/rufio72/blitx3d_linux/_release`;
`BCC=/home/rufio72/blitx3d_linux/_release/bin/blitzcc`; `blitzcc` vuole `-o`
PRIMA del sorgente; per i test grafici usa il display reale (es. `DISPLAY=:1`).
Le uscite segnate «attesa, non misurata» sono dedotte dal codice: in questo
ambiente non si può compilare né eseguire; lo sviluppatore le misura.

1. **Misura dello stato PRIMA del fix** (obbligatoria, da riportare):
   `ninja -C build/x86_64-linux-release blitzcc`, poi crea i quattro file di
   test negativi e compila ciascuno con `$BCC -c <file>`; infine
   `cd test && $BCC -r test -o $TMP/all_test all.bb && $TMP/all_test | tail -1`.
   Attesa, non misurata: `access-private-function-self.bb`,
   `access-private-inherited.bb` e `access-super-private.bb` oggi COMPILANO
   (bug I1), `access-private-outside.bb` oggi fallisce; suite
   `Pass: 482. Fail: 17` (i 17 Fail sono ambientali). Annota i numeri reali:
   sono la baseline delle verifiche 6-7.
2. Dopo il fix: `ninja -C build/x86_64-linux-release blitzcc` → exit 0, nessun
   warning nuovo sui file toccati.
3. `$BCC -c test/syntax-errors/access-private-function-self.bb` → exit ≠ 0,
   output contiene `Cannot access private field` (attesa, non misurata).
4. `$BCC -c test/syntax-errors/access-private-inherited.bb` → exit ≠ 0,
   `Cannot access private field` (attesa, non misurata; dipende da Q1).
5. `$BCC -c test/syntax-errors/access-super-private.bb` → exit ≠ 0,
   `Cannot access private method` (attesa, non misurata; dipende da Q3).
6. `$BCC -c test/syntax-errors/access-private-outside.bb` → exit ≠ 0, come
   nella misura PRIMA (nessuna regressione).
7. Suite: `cd test && $BCC -r test -o $TMP/all_test all.bb && $TMP/all_test | tail -1`
   → `Pass: 487. Fail: 17`, cioè baseline misurata in (1) con Pass +5 e Fail
   invariati (attesa, non misurata).
8. Samples OOP invariati: per ciascuno di `test_super`, `test_inheritance`,
   `test_multi_inheritance` in `_release/samples/oop/`:
   `$BCC -o $TMP/<nome> _release/samples/oop/<nome>.bb && $TMP/<nome> | diff - _release/samples/oop/<nome>_result.txt`
   → diff vuoto, exit 0 (attesa, non misurata).
9. `wc -l src/tools/compiler/environ.{h,cpp} src/tools/compiler/tree/var/field_var.cpp src/tools/compiler/tree/decl/{class_decl,func_decl}.cpp src/tools/compiler/tree/expr/method_call.cpp`
   → tutti sotto le 800 righe.
10. `git status --short` → solo i file elencati nella sezione 4 (più questo
    brief se aggiornato con gli esiti).

## 6. Consegna

- Commit dei soli file del fix (sorgenti sezione 4 + test nuovi +
  `test/language.bb`), messaggio in inglese, per esempio
  `Compiler: real class context for Private/Protected access control (I1)`,
  con riga finale `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  Nessun push.
- Rapporto finale: massimo trenta righe, con i numeri misurati in (1), l'esito
  delle verifiche 2-10 e le deviazioni dal brief.

## 7. Domande aperte

- **Q1 — campo `Private` ereditato: visibile alla sottoclasse?** Proposta: NO
  (convenzione Java e C++: `private` non è accessibile dalle classi derivate,
  anche se il campo è fisicamente copiato nel figlio). Chi vuole condividere
  col figlio usa `Protected`. Marca: **preferenza**.
- **Q2 — check `Protected` e tipo statico.** Proposta: l'accesso dipende solo
  da `currentClass->isSubclassOf( ownerClass )`, il tipo statico
  dell'espressione è irrilevante (stile C++; si rinuncia alla restrizione
  Java "solo attraverso riferimenti del proprio sottotipo", più complessa e
  sorprendente). Corregge anche il caso oggi rotto del metodo del padre su
  espressione tipata figlio. Marca: **preferenza**.
- **Q3 — `Super` verso un metodo `Private` del padre.** Proposta: errore
  (Java e C++ non permettono di chiamare un `private` del padre dalla
  sottoclasse); `Protected` e pubblici restano chiamabili. Marca:
  **preferenza**.
- **Q4 — `Static Method` e privati della propria classe.** Proposta: pieno
  accesso attraverso un oggetto della classe (come in Java/C++); oggi non
  accede a nulla. Marca: **preferenza**.
- **Q5 — chiamate col nome manglato `ClassName_MethodName(...)` (CallNode).**
  Oggi bypassano ogni check: un metodo `Private` è chiamabile da chiunque come
  funzione globale. Però il nome manglato è anche l'UNICA sintassi per
  chiamare gli Static Method, quindi aggiungere i check cambia il linguaggio
  (uno Static `Private` diventa chiamabile solo dalla propria classe, un
  metodo d'istanza `Private` non è più chiamabile come funzione). Proposta:
  applicare i check anche lì (modifica 7); i membri pubblici restano
  chiamabili come oggi. Marca: **scelta di progetto**.
