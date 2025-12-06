; Test Super keyword for calling parent class methods

; Base class Animal
Class Animal
	Field name$

	Method Init(n$)
		Self\name$ = n$
	End Method

	Method GetGreeting$()
		Return "I am an animal named " + Self\name$
	End Method
End Class

; Derived class Dog
Class Dog Extends Animal
	Field breed$

	Method Init(n$, b$)
		; Call parent's Init first
		Super\Init(n$)
		Self\breed$ = b$
	End Method

	Method GetFullGreeting$()
		; Call parent's GetGreeting
		Local base$ = Super\GetGreeting$()
		Return base$ + " and I am a " + Self\breed$
	End Method
End Class

; Create a dog instance
Local d.Dog = New Dog()
d\Init("Rex", "German Shepherd")

; Get the full greeting (which uses Super)
Local greeting$ = d\GetFullGreeting$()

; Write results to file
Local f = WriteFile("test_super_result.txt")
WriteLine f, "Super keyword test results:"
WriteLine f, greeting$
WriteLine f, "Test completed successfully!"
CloseFile f

; Also print the result
Print greeting$
Print "Test completed successfully!"
