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
