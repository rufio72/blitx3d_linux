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
