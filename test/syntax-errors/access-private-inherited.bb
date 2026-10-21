Class AccIBase
	Private Field segreto
End Class

Class AccIFiglia Extends AccIBase
	Method Leggi()
		Return Self\segreto
	End Method
End Class

Local f.AccIFiglia = New AccIFiglia()
Print f\Leggi()
