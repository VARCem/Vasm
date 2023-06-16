/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle any errors.
 *
 * Version:	@(#)error.c	1.0.4	2023/06/05
 *
 * Authors:	Fred N. van Kempen, <waltje@varcem.com>
 *		Bernd B”ckmann, <https://codeberg.org/boeckmann/asm6502>
 *
 *		Copyright 2023 Fred N. van Kempen.
 *		Copyright 2022,2023 Bernd B”ckmann.
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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#if __STDC_VERSION__ >= 201112L
# include <stdnoreturn.h>
#else
# define noreturn /*NORETURN*/
#endif
#include <string.h>
#include <setjmp.h>
#define HAVE_SETJMP_H
#include "error.h"


int		errors;
jmp_buf		error_jmp;
char		error_hint[128];
const char	*err_msgs[ERR_MAXERR] = {
    "no error",
    "fatal",
    "division by zero detected",
    "processor type not set",
    "unknown processor type",
    "out of memory",
    "assert failed",
    "can not create file",
    "can not open file",
    "file format not enabled"
    "unknown directive",
    "unknown instruction",
    "comma expected",
    "value expected",
    "invalid format specifier",
    "error in expression",
    "incomplete operator",
    "unbalanced parentheses",
    "label required",
    "label not valid here",
    "identifier expected",
    "identifier length exceeded",
    "statement expected",
    "illegal statement",
    "end of line expected",
    "illegal redefinition",
    "IF nesting too deep",
    "ELSE without IF",
    "ENDIF without IF",
    "too many REPEAT levels",
    "ENDREP without REPEAT",
    "REPEAT without ENDREP",
    "symbol already defined as label",
    "missing closing brace",
    "undefined value",
    "illegal type",
    "string not terminated",
    "character constant not terminated",
    "value out of range",
    "byte value out of range",
    "word value out of range",
    "illegal redefinition of local label",
    "local label definition requires previous global label",
    "malformed character constant",
    "string too long",
    "string expected",
    "maximum number of include files reached",
};




/* Display error message and abort action. */
noreturn void
error(int err, const char *msg)
{
    errors++;

    if (msg != NULL)
	strncpy(error_hint, msg, sizeof(error_hint) - 1);
    else
	memset(error_hint, 0x00, sizeof(error_hint));

    longjmp(error_jmp, err);
    /*NOTREACHED*/
}
