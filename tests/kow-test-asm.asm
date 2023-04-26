;KOWALSKI ASSEMBLER LOGICAL, BITWISE & ARITHMETIC OPERATORS
;
;
;   number radices...
;
;       %     binary, e.g., %01011010
;       $     hex, e.g., $5a
;       none  decimal
;

;   .opt proc65c02,caseinsensitive

test0001 = %00001111
.assert test0001 == $0F

test0002 = test0001 << 4	; logical shift left 4 bits
.assert test0002 == $F0

test0003 = test0002 >> 4	; logical shift right 4 bits
.assert test0003 == $0F

test0004 = test0001 & test0002	; bitwise AND
.assert test0004 == $00

test0005 = test0001 | test0002	; bitwise OR
.assert test0005 == $FF

test0006 = test0001 && test0002	; logical AND
.assert test0006 == 1

test0007 = test0001 || test0002	; logical OR
.assert test0007 == 1

test0008 = !0			; bitwise NOT

test0009 = $5a ^ test0005	; bitwise XOR

test0010 = 4 == 3		; equality test
.assert test0010 == 0

test0011 = 4 == 4		; equality test
.assert test0011 == 1

test0012 = 4 != 3		; inequality test
.assert test0012 == 1

test0013 = 4 != 4		; inequality test
.assert test0013 == 0

test0014 = 4 > 3		; greater-than test
.assert test0014 == 1

test0015 = 4 < 3		; less-than test
.assert test0015 == 0

test0016 = ~1			; 2s complement

;
;
;   arithmetic operators...
;
;       +   addition
;       -   subtraction
;       *   multiplication
;       /   division
;       %   modulo
;
sum      = 5   + 6             ;evaluates to 11
.assert sum == 11
diff     = sum - 6             ;evaluates to 5
.assert diff == 5
prod     = 5   * 6             ;evaluates to 30
.assert prod == 30
quot     = prod / diff         ;evaluates to 6
.assert quot == 6
mod      = prod % sum          ;evaluates to 8
.assert mod == 8
;
;
;   example using square brackets to alter evaluation precedence...
;
test0017 =5 + 6 * 2            ;strictly left-to-right: evaluates to 17
test0018 =(5 + 6) * 2          ;sum of 5 & 6 computed 1st: evaluates to 22
;

	nop

	.end
