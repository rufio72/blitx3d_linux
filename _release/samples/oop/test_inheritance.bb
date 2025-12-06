; Test method inheritance

Class Animal
	Field name$

	Method SetName(n$)
		Self\name$ = n$
	End Method

	Method GetName$()
		Return Self\name$
	End Method
End Class

Class Dog Extends Animal
	Field breed$

	Method SetBreed(b$)
		Self\breed$ = b$
	End Method
End Class

; Create a dog
Local d.Dog = New Dog()

; This should work - calling inherited method
d\SetName("Rex")

; Write result
Local f = WriteFile("test_inheritance_result.txt")
WriteLine f, "Name: " + d\GetName$()
WriteLine f, "Test completed!"
CloseFile f
