; Test multi-level method inheritance

Class Animal
	Field name$

	Method SetName(n$)
		Self\name$ = n$
	End Method

	Method GetName$()
		Return Self\name$
	End Method

	Method Speak$()
		Return "..."
	End Method
End Class

Class Dog Extends Animal
	Field breed$

	Method SetBreed(b$)
		Self\breed$ = b$
	End Method

	Method GetBreed$()
		Return Self\breed$
	End Method

	Method Speak$()
		Return "Woof!"
	End Method
End Class

Class GermanShepherd Extends Dog
	Field training$

	Method SetTraining(t$)
		Self\training$ = t$
	End Method

	Method GetInfo$()
		; Uses inherited methods from both Dog and Animal
		Return Self\GetName$() + " is a " + Self\GetBreed$() + " trained in " + Self\training$
	End Method
End Class

; Create a German Shepherd
Local gs.GermanShepherd = New GermanShepherd()

; Call inherited methods from Animal
gs\SetName("Rex")

; Call inherited method from Dog
gs\SetBreed("German Shepherd")

; Call own method
gs\SetTraining("police work")

; Test overridden method (should use Dog's Speak, not Animal's)
Local bark$ = gs\Speak$()

; Write results
Local f = WriteFile("test_multi_result.txt")
WriteLine f, "Info: " + gs\GetInfo$()
WriteLine f, "Speak: " + bark$
WriteLine f, "Test completed!"
CloseFile f
