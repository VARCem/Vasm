; Preamble to create a C64 .PRG file.

SYS	= $9E			; basic SYS token number

LOAD	= $0801			; load address

	.cpu	6510		; set processor for C64

	.word	LOAD		; .PRG header: load address

	.nofill			; make sure we do not fill
	.org	LOAD		; we start at this address

basic:				; BASIC code: 10 SYS 2062
        .word @end, 10          ; ptr to next basic line and line number 10
        .byte SYS, " 2062", 0   ; SYS token and address string of subroutine
@end:   .word 0                 ; null ptr to indicate end of basic text

; End of preamble code.
