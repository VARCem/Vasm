; C64 Hello World

	.ifdef C64
	.include "tests/c64_prg.asm"
	.else
	.cpu	6502
	.org	$0400
	.endif

; assemble to .PRG file: vasm -o hello.prg c64.asm c64_hello.asm

CHROUT = $FFD2                  ; kernal function address
CR     = 13                     ; carrige return character
LF     = %1010                  ; line feed character

main:				; this is at address 2062 ($080E)
	ldx	#0
@l	lda	msg, x
	jsr	CHROUT
	inx
	cpx	#len
	bne	@l
	rts

msg	.byte	"HELLO, WORLD!", CR, LF
len	= @ - msg

; End of file.
