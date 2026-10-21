Class Cassaforte
	Private Field codiceSegreto
End Class

Function Ruba%(self.Cassaforte)
	Return self\codiceSegreto
End Function

Local c.Cassaforte = New Cassaforte()
Print Ruba(c)
