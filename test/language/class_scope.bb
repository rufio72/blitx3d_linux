; Visibilita' di Global, Dim e Function dentro i Method (finding C2)

Global ClassScopeCount
Dim ClassScopeTable(3)

Class ClassScopeGame
	Field n

	Method Add()
		ClassScopeCount = ClassScopeCount + 1
		ClassScopeTable(1) = ClassScopeTable(1) + 10
	End Method

	Method GetCount()
		Return ClassScopeCount
	End Method

	Method Double(v)
		Return ClassScopeHelper(v)
	End Method

	Method Chain(v)
		Return Self\Double(v) + 1
	End Method

	Static Method Count()
		Return ClassScopeCount
	End Method
End Class

Function ClassScopeHelper(x)
	Return x * 2
End Function

Local classScopeObj.ClassScopeGame = New ClassScopeGame()
classScopeObj\Add()
classScopeObj\Add()

Expect ClassScopeCount = 2, "a Method increments a Global"
Expect ClassScopeTable(1) = 20, "a Method writes a Dim array"
Expect classScopeObj\GetCount() = 2, "a Method reads a Global"
Expect ClassScopeGame_Count() = 2, "a Static Method reads a Global"
Expect classScopeObj\Double(21) = 42, "a Method calls a Function declared after the Class"
Expect classScopeObj\Chain(5) = 11, "a Method calls another Method through Self"
