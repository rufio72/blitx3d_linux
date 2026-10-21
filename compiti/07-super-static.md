# Compito 07: Super e Self dentro uno Static Method (finding I8)

Data 2026-10-21. Scritto dall'agente analista per un agente sviluppatore.
Questo file è tutto il contesto che serve: non c'è una chat da leggere.

## 1. Contesto e obiettivo

- Nel compilatore `blitzcc` i metodi sono funzioni globali `Classe_Metodo(self, ...)`; uno `Static Method` non ha il parametro `self`.
- Dentro uno Static Method il parser accetta comunque `Super\M()` e `Self`, perché imposta lo stesso `currentClassName` dei metodi normali.
- `SuperMethodCallNode::semant` cerca `self` con `Environ::findDecl`, che trova anche una `Global self` del programma: il codice generato le passa come `self` un valore del tipo sbagliato. Senza `Global self` l'errore c'è ma dice "Super can only be used inside a method", che è fuorviante.
- `Self` dentro uno Static Method, invece, compila sempre senza errori: o legge la `Global self`, o dichiara in silenzio una Local intera `self` che vale 0.
- Obiettivo: un errore di compilazione chiaro per `Super` e per `Self`/`This` dentro uno Static Method, senza cambiare il comportamento dei metodi normali né quello del codice classico che usa `self` come identificatore fuori dalle classi.

Vincoli assoluti:
- Modificare solo i file elencati nella sezione 3, tutti sotto `src/tools/compiler/`.
- `src/tools/compiler/tree/parser.cpp` ha **1199 righe** (già oltre 800): non va accresciuto. Per questo il fix si fa in semant e non in parse (vedi la domanda aperta 1).
- Nessun file di codice sopra le 800 righe.
- I file `.bb` di prova vanno in una cartella temporanea (`TMP=$(mktemp -d)`), non nel repository.
- Commenti nel codice in inglese, come nel resto del compilatore. Messaggi d'errore in inglese.

## 2. Fatti verificati

Percorsi relativi a `src/tools/compiler/`. Le righe vengono da `sed -n`/`grep -n` su `b3c4aa0`.

| Fatto | Dove |
|---|---|
| Kind delle dichiarazioni: `DECL_FUNC=1, DECL_ARRAY=2, DECL_STRUCT=4, DECL_GLOBAL=8, DECL_LOCAL=16, DECL_PARAM=32, DECL_FIELD=64` | `tree/decl.h:13-14` |
| `Environ::findDecl` risale la catena `globals`: Local e Param li accetta solo nell'environ corrente (`e==this`), tutto il resto (Global) lo accetta a qualunque livello. Quindi da dentro un metodo trova una `Global self` del programma | `environ.cpp:26-35` |
| `Environ::getCurrentClass()` cerca `"self"` **solo** in `decls` dell'environ corrente e restituisce `self->type->structType()` solo se `kind & DECL_PARAM`, altrimenti 0. È esattamente il controllo che manca a Super | `environ.cpp:69-77`, dichiarata in `environ.h:46` |
| Costruttore `Environ(const std::string &f, Type *r, int l, Environ *gs)`; i campi della classe sono in `environ.h:19-31` (nessun campo sul metodo corrente) | `environ.cpp:5-11`, `environ.h:33` |
| `parseMethodDecl(className, isStatic, accessMod)`: aggiunge il parametro `VarDeclNode("self", className, DECL_PARAM, ...)` solo se `!isStatic` | `tree/parser.cpp:842-860` |
| Ogni metodo, statico o no, imposta `currentClassName = className` per tutto il corpo e poi lo ripristina | `tree/parser.cpp:873-877` |
| Lo Static Method viene da `parseMethodDecl( ident, true, accessMod )` | `tree/parser.cpp:819-824` |
| Il metodo diventa `FuncDeclNode( funcName, tag, params, stmts, accessMod, className )`: `ownerClassName` non è vuoto per tutti i metodi, statici compresi. Le Function normali (`parser.cpp:762`) hanno `ownerClassName` vuoto | `tree/parser.cpp:883`, `tree/decl/func_decl.h:14-15` |
| `FuncDeclNode::semant`: crea `sem_env = d_new Environ(genLabel(), sem_type->returnType, 1, e)`, vi inserisce i parametri con il loro kind (`self` come `DECL_PARAM`) e poi fa la semant del corpo con `sem_env` | `tree/decl/func_decl.cpp:26-37` |
| Gli unici `d_new Environ` sono `libs.cpp:319`, `tree/prognode.cpp:14` e `tree/decl/func_decl.cpp:28`: nessun environ annidato per blocco, quindi dentro il corpo di un metodo l'environ è sempre il `sem_env` del metodo | grep |
| Statement `Self`/`This`: se `currentClassName` è vuoto ricade su identificatore (`goto this_as_ident_stmt`), altrimenti `parseVarOrMethodCall("self", "", true)` | `tree/parser.cpp:247-271` |
| Espressione `Self`/`This`: stessa logica | `tree/parser.cpp:1148-1157` |
| `Super` come statement e come espressione: unico controllo `currentClassName.empty()` → `ex("Super can only be used inside a method")`, poi `SuperMethodCallNode(currentClassName, ...)` | `tree/parser.cpp:272-292` e `1158-1178` |
| `parseVarOrMethodCall` costruisce sempre `IdentVarNode(ident, tag)` come radice (`self`), con sopra `FieldVarNode` o `MethodCallNode` | `tree/parser.cpp:634-641` |
| `IdentVarNode::semant`: `e->findDecl(ident)`; se non trova nulla dichiara in silenzio una `DECL_LOCAL` del tipo del tag (int di default) | `tree/var/ident_var.cpp:6-21` (file di 29 righe) |
| `SuperMethodCallNode::semant`: cerca la classe, il genitore, il metodo; solo dopo, alla riga 188, `self_decl = e->findDecl( "self" )` senza controllare kind né tipo; riga 189 `ex( "Super can only be used inside a method" )` | `tree/expr/method_call.cpp:164-208` |
| Uso di `self_decl` nei backend: x86 `local(self_decl->offset)` (`:217`), LLVM `CreateLoad(self_decl->type->llvmType(...), self_decl->ptr)` (`:245`), C con nomi diversi per GLOBAL/LOCAL/PARAM (`:266-270`) | `tree/expr/method_call.cpp` |
| Il backend x86 rifiuta già i programmi con Class (commit C1, `b3c4aa0`) | `tree/prognode.cpp` |
| Stile dei messaggi esistenti: "Select cannot be used with objects", "'Before' cannot be used with 'Null'", "Cannot access private method '...'" | `tree/stmt/select.cpp:17`, `tree/expr/before.cpp:8`, `tree/expr/method_call.cpp:41` |
| Righe dei file da toccare: `method_call.cpp` 294, `ident_var.cpp` 29, `environ.h` 49, `environ.cpp` 77, `func_decl.cpp` 168 | `wc -l` |
| `blitzcc` in locale è compilato **senza LLVM** (`-llvm` risponde "not compiled with llvm support"): il backend di default è quello C (GCC) | misurato |
| La build scrive `blitzcc` direttamente in `_release/bin/` (`RUNTIME_OUTPUT_DIRECTORY`, `RELEASE_PATH ${CMAKE_SOURCE_DIR}/_release`) | `CMakeLists.txt:279-300` |
| Nella suite non ci sono test con `Super\` né test di errori di compilazione dentro `all.bb` (un errore di compilazione ferma tutta la suite). Static Method coperto in positivo da `test/language/class_scope.bb:26,42` | grep |

### Stato prima del fix (misurato il 2026-10-21)

Misurato con `_release/bin/blitzcc` costruito da `b3c4aa0`, comando `blitzcc -r test -o $TMP/tN $TMP/tN.bb`. I sorgenti sono nella sezione 4.

| Prova | Compilazione | Note |
|---|---|---|
| t1 (`Global self = 7`, Super in Static Method) | exit 0 | warning gcc: `passing argument 1 of 'bb_fssbase_hello' makes pointer from integer without a cast [-Wint-conversion]`: l'intero 7 viene passato come self |
| t2 (Super in Static Method, nessuna global) | exit 255 | `"t2_noglobal.bb":8:3:8:3:Super can only be used inside a method` |
| t3 (`Print "self=" + Self` in Static Method) | exit 0 | nessun errore: `self` diventa una Local intera |
| t4 (`Global self.SSChild`, Super in Static Method) | exit 0 | nessun errore: la Global viene usata come self |
| t5 (caso valido: Super in Method normale + Static Method senza Self) | exit 0 | deve restare così |

## 3. Modifiche richieste

### 3a. `tree/expr/method_call.cpp`: Super (obbligatoria)

In `SuperMethodCallNode::semant`, **come primo controllo** (prima di `e->findType( currentClass )`, così l'errore non viene coperto da "has no parent class" o "Method ... not found"):
- `StructType *self_class = e->getCurrentClass();` Se è 0, siamo in uno Static Method (il parser crea `SuperMethodCallNode` solo con `currentClassName` non vuoto): `ex( "Super cannot be used inside a Static Method" )`.
- `self_decl = e->decls->findDecl( "self" );` (cerca solo nell'environ corrente: dopo `getCurrentClass()` è garantito un `DECL_PARAM` di tipo struct).
- Togli le righe 187-189 (vecchio `findDecl` più errore).
- Non toccare i tre `translate*`. I rami GLOBAL e LOCAL di `translate3` (`:267-268`) diventano irraggiungibili: lasciali (domanda 3).

### 3b. `Self`/`This` dentro uno Static Method (dipende dalla domanda 1, proposta A)

Il parser non distingue lo Static Method nel punto in cui legge `Self`, e `parser.cpp` non si può accrescere. Quindi il controllo va in semant, con un'informazione nuova sull'environ:
- `environ.h`: aggiungi alla classe `Environ` il campo `bool staticMethod;` con un commento di una riga ("true while analysing the body of a Static Method").
- `environ.cpp`: inizializzalo a `false` nel costruttore (`:5-11`).
- `tree/decl/func_decl.cpp`, `FuncDeclNode::semant`: dopo il ciclo che inserisce i parametri (`:31-34`) e prima di `stmts->semant( sem_env )`, imposta `sem_env->staticMethod = !ownerClassName.empty() && !sem_env->getCurrentClass();`.
- `tree/var/ident_var.cpp`, `IdentVarNode::semant`: subito dopo `if( sem_decl ) return;`, se `e->staticMethod` e `ident=="self"`, allora `ex( "Self cannot be used inside a Static Method" )`. Dentro uno Static Method `self` è sempre il token `SELF`/`THIS` (perché `currentClassName` non è vuoto), quindi non si rompe il codice classico che usa `self` come nome fuori dalle classi. Il toker porta in minuscolo sia gli IDENT sia le parole chiave di classe (`tree/toker.cpp:198,204-206`), e `parseVarOrMethodCall` riceve il letterale `"self"`: il confronto esatto con `"self"` basta.

Nient'altro: nessuna modifica a `parser.cpp`, `parser.h`, `findDecl`, ai test della suite o alla documentazione.

## 4. Verifiche obbligatorie

Ambiente comune:
```bash
cd /home/rufio72/blitx3d_linux
export blitzpath=/home/rufio72/blitx3d_linux/_release
export PATH=$blitzpath/bin:$PATH
TMP=$(mktemp -d)
```
`blitzcc` vuole `-o` **prima** del sorgente. Crea in `$TMP` i cinque file qui sotto (tab o spazi, è indifferente).

`t1_global.bb`:
```
Global self = 7
Class SSBase
	Method Hello()
		Print "base hello"
	End Method
End Class
Class SSChild Extends SSBase
	Static Method Make()
		Super\Hello()
	End Method
End Class
SSChild_Make()
```
`t2_noglobal.bb`: come t1 senza la prima riga.
`t3_self.bb`:
```
Class SSThing
	Field n
	Static Method Show()
		Print "self=" + Self
	End Method
End Class
SSThing_Show()
```
`t4_globalobj.bb`: come t2, con `Field n` in SSBase, `Print "base hello n=" + Self\n` in `SSBase.Hello`, e dopo le classi `Global self.SSChild = New SSChild()`, `self\n = 42`, `SSChild_Make()`.
`t5_ok.bb`:
```
Class SSBase
	Method Hello()
		Print "base hello"
	End Method
End Class
Class SSChild Extends SSBase
	Method Hello()
		Super\Hello()
		Print "child hello"
	End Method
	Static Method Make()
		Print "static ok"
	End Method
End Class
c.SSChild = New SSChild()
c\Hello()
SSChild_Make()
```

1. **Stato prima del fix** (da fare prima di toccare il codice, con il binario attuale):
   `for t in t1_global t2_noglobal t3_self t4_globalobj t5_ok; do blitzcc -r test -o $TMP/$t $TMP/$t.bb >$TMP/$t.log 2>&1; echo "$t exit=$?"; done; grep -h "Super\|Self\|int-conversion" $TMP/*.log`
   Attesa (misurata il 2026-10-21): t1, t3, t4 e t5 con `exit=0`, t2 con `exit=255`; nel grep il warning `-Wint-conversion` di t1 e la riga `"t2_noglobal.bb":8:3:8:3:Super can only be used inside a method`. Se differisce, fermati e riportalo.
2. **Build**: `ninja -C build/x86_64-linux-release blitzcc; echo exit=$?` → `exit=0`, senza errori, e `ls -la $blitzpath/bin/blitzcc` con l'ora appena aggiornata. (Attesa, non misurata.)
3. **Super in Static Method con Global self**: `blitzcc -r test -o $TMP/t1 $TMP/t1_global.bb; echo exit=$?` → riga `"t1_global.bb":9:...:Super cannot be used inside a Static Method` ed exit diverso da 0 (255). (Attesa, non misurata.)
4. **Super senza global**: lo stesso con `t2_noglobal.bb` → `"t2_noglobal.bb":8:...:Super cannot be used inside a Static Method`, exit 255. (Attesa, non misurata.)
5. **Super con Global oggetto**: lo stesso con `t4_globalobj.bb` → `Super cannot be used inside a Static Method`, exit 255. (Attesa, non misurata.)
6. **Self in Static Method** (solo se si applica la 3b): lo stesso con `t3_self.bb` → `"t3_self.bb":4:...:Self cannot be used inside a Static Method`, exit 255. (Attesa, non misurata.) La colonna può differire: conta il testo.
7. **Caso valido invariato**: `blitzcc -r test -o $TMP/t5 $TMP/t5_ok.bb; echo exit=$?` → `exit=0`, senza errori né warning. Poi compila anche `_release/samples/oop/test_super.bb`, `test_inheritance.bb` e `test_multi_inheritance.bb` con `blitzcc -o $TMP/x <file>`: exit 0 per tutti e tre. (Attesa, non misurata.)
8. **Suite completa**: `cd test && blitzcc -r test -o $TMP/all_test all.bb && $TMP/all_test 2>&1 | tail -3` → `Pass: 482. Fail: 17` (i 17 fallimenti dipendono dall'ambiente e ci sono già). Se serve il display, usa `DISPLAY=:1`. (Attesa, non misurata.)
9. **Ambito**: `git status --short` → solo `src/tools/compiler/tree/expr/method_call.cpp` e, con la 3b, `src/tools/compiler/environ.h`, `src/tools/compiler/environ.cpp`, `src/tools/compiler/tree/decl/func_decl.cpp`, `src/tools/compiler/tree/var/ident_var.cpp`. Nessun `.bb` nel repo.
10. **Dimensione dei file**: `wc -l src/tools/compiler/tree/expr/method_call.cpp src/tools/compiler/environ.h src/tools/compiler/environ.cpp src/tools/compiler/tree/decl/func_decl.cpp src/tools/compiler/tree/var/ident_var.cpp src/tools/compiler/tree/parser.cpp` → tutti sotto 800 tranne `parser.cpp`, che resta esattamente 1199.

## 5. Vincoli e consegna

- Nessun file di codice sopra le 800 righe; `parser.cpp` (1199) non va toccato.
- Commit solo dei file del fix, messaggio in inglese, per esempio:
  `Compiler: reject Super and Self inside Static Method (I8)`, con un corpo breve e come ultima riga
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Nessun push.
- Rapporto finale di massimo trenta righe: uscita delle verifiche 1-10 (exit e righe d'errore vere) e ogni deviazione dal brief.

## 6. Domande aperte

1. **Self/This dentro uno Static Method: fix in semant (3b), in parse o rinviato?** «scelta di progetto».
   Proposta: A, cioè la 3b in semant (campo `Environ::staticMethod` più controllo in `IdentVarNode`), senza toccare `parser.cpp`.
   Alternative:
   - B: controllo nel parser (un flag `currentMethodStatic` in `parser.h` e quattro controlli in `parser.cpp:247,272,1148,1158`). È più pulito ma accresce di circa dieci righe un file già a 1199, contro la regola delle 800 righe.
   - C: solo Super ora (3a); Self resta a un compito successivo, dopo aver spezzato `parser.cpp`.
2. **Self in Static Method: errore o identificatore libero come fuori dalle classi?** «scelta di progetto». Proposta: errore, perché il codice nuovo dentro una classe non deve leggere per sbaglio una Global `self`. L'effetto collaterale è che `Local self` dichiarata in uno Static Method non si può più leggere.
3. **Rami GLOBAL e LOCAL di `SuperMethodCallNode::translate3` (`method_call.cpp:267-268`), ora irraggiungibili.** «preferenza». Proposta: lasciarli, fuori ambito.
4. **Testo dei messaggi.** «preferenza». Proposta: "Super cannot be used inside a Static Method" e "Self cannot be used inside a Static Method"; anche con `This` il messaggio dice `Self`, che è il nome principale.
5. **Test di regressione nel repo.** «preferenza». Proposta: nessuno. La suite `all.bb` non supporta test che devono fallire in compilazione, e `test/syntax-errors/` viene eseguita da `suite.sh` senza controllarne l'esito.
