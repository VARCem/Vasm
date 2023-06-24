/*
 * VASM		VARCem Multi-Target Macro Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the error codes.
 *
 * Version:	@(#)error.h	1.0.6	2023/06/23
 *
 * Author:	Fred N. van Kempen, <waltje@varcem.com>
 *
 *		Copyright 2023 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ERROR_H
# define ERROR_H


typedef enum errors_e {
    ERR_USER = 1,		// user-specified error
    ERR_ZERO,			// "division by zero"
    ERR_NOCPU,			// "processor type not set"
    ERR_CPU,			// "unknown processor type"
    ERR_MEM,			// "out of memory"
    ERR_ASSERT,			// "assert failed"
    ERR_CREATE,			// "can not create file"
    ERR_OPEN,			// "can not open file"
    ERR_NO_FMT,			// "file format not enabled"
    ERR_NODIRECTIVE,		// "unknown directive"
    ERR_INSTR,			// "unknown instruction"
    ERR_COMMA,			// "comma expected"
    ERR_NUM,			// "value expected"
    ERR_VALUE,			// "invalid value"
    ERR_FMT,			// "invalid format specifier"
    ERR_EXPR,			// "error in expression"
    ERR_OPER,			// "incomplete operator"
    ERR_UNBALANCED,		// "unbalanced parentheses"
    ERR_LABEL,			// "label required"
    ERR_NOLABEL,		// "label not valid here"
    ERR_ID,			// "identifier expected"
    ERR_IDLEN,			// "identifier length exceeded"
    ERR_STMT,			// "statement expected",
    ERR_NOSTMT,			// "illegal statement"
    ERR_EOL,			// "end of line expected"
    ERR_REDEF,			// "illegal redefinition"
    ERR_MACACT,			// "not enough actual params"
    ERR_MACFRM,			// "not enough formal params"
    ERR_MACRO,			// "MACRO before ENDM",
    ERR_ENDM,			// "ENDM before MACRO",
    ERR_IF,			// "IF nesting too deep"
    ERR_ELSE,			// "ELSE without IF"
    ERR_ENDIF,			// "ENDIF without IF"
    ERR_MAX_REP,		// "too many REPEAT levels"
    ERR_REPEAT,			// "ENDREP without REPEAT"
    ERR_ENDREP,			// "REPEAT without ENDREP"
    ERR_LBLREDEF,		// "symbol already defined as label"
    ERR_CLBR,			// "missing closing brace"
    ERR_UNDEF,			// "undefined value"
    ERR_ILLTYPE,		// "illegal type"
    ERR_STREND,			// "string not terminated"
    ERR_CHREND,			// "character constant not terminated"
    ERR_RNG,			// "value out of range"
    ERR_RNG_BYTE,		// "byte value out of range
    ERR_RNG_WORD,		// "word value out of range
    ERR_LOCAL_REDEF,		// "illegal redefinition of local label"
    ERR_NO_GLOBAL,		// "local label definition .. global label"
    ERR_CHR,			// "malformed character constant"
    ERR_STRLEN,			// "string too long"
    ERR_STR,			// "string expected"
    ERR_MAXINC,			// "maximum number of include files reached"

    ERR_MAXERR			// last generic error
} errors_t;


/* Global variables. */
extern int		errors;
#ifdef HAVE_SETJMP_H
extern jmp_buf		error_jmp;
#endif
extern char		error_hint[];
extern const char	*err_msgs[];


/* Functions. */
extern void	error(int, const char *);


#endif	/*ERROR_H*/
