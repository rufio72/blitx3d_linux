
Context "Literals"

fourOhNineFive = $fff
Expect fourOhNineFive = 4095, "$fff = 4095"

sos = %1001001
Expect sos = 73, "1001001 = 73"

oneHalf# = .5 ; the lack of a zero here is intentional
Expect oneHalf < 1 And oneHalf > 0, ".5 is between 0 and 1"

big = 5000000000
Expect big / 1000 = 5000000, "5000000000 is a 64-bit literal"
Expect 2000000000 + 2000000000 > 0, "constant folding does not wrap at 32 bits"
Expect Str(-5000000000) = "-5000000000", "negative 64-bit literal"
Expect $FFFFFFFF > 0, "$FFFFFFFF is 4294967295"
