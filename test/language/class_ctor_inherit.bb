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
