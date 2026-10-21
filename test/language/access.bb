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
