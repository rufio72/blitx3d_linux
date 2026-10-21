; Firme dei Method che citano classi dichiarate dopo (finding I2)

Class ClassFwdA
	Field v

	Method MakeB.ClassFwdB(n)
		Local b.ClassFwdB = New ClassFwdB()
		b\SetW(n)
		Return b
	End Method

	Method ReadB(b.ClassFwdB)
		Return b\GetW() + Self\v
	End Method
End Class

Class ClassFwdB
	Field w

	Method SetW(n)
		Self\w = n
	End Method

	Method GetW()
		Return Self\w
	End Method

	Method MakeA.ClassFwdA(n)
		Local a.ClassFwdA = New ClassFwdA()
		a\v = n
		Return a
	End Method
End Class

Class ClassFwdC Extends ClassFwdA
	Method Twice(b.ClassFwdB)
		Return Self\ReadB(b) * 2
	End Method
End Class

Local classFwdA.ClassFwdA = New ClassFwdA()
Local classFwdB.ClassFwdB = classFwdA\MakeB(7)
Local classFwdA2.ClassFwdA = classFwdB\MakeA(3)
Local classFwdC.ClassFwdC = New ClassFwdC()
classFwdC\v = 1

Expect classFwdB\GetW() = 7, "a Method returns a Class declared after its own"
Expect classFwdA\ReadB(classFwdB) = 7, "a Method takes a Class declared after its own"
Expect classFwdA2\v = 3, "a Method returns a Class declared before its own"
Expect classFwdC\Twice(classFwdB) = 16, "an inherited Method works with a later Class"
