/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle selection of a target device.
 *
 * Version:	@(#)target.c	1.0.1	2023/04/20
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "global.h"
#include "target.h"


extern const target_t	t_6502_old,
			t_6502_nmos,
			t_csg6510,
			t_csg8500,
			t_65c02;


static const target_t *targets[] = {
    &t_6502_old,		// original MOS6502 (NMOS)
    &t_6502_nmos,		// standard MOS6502 (NMOS)
    &t_csg6510,			// CSG 6510 (NMOS)
    &t_csg8500,			// CSG 8500 (NMOS)

    &t_65c02,			// standard MOS65C02 (CMOS)

    NULL
};

static const target_t	*target = NULL;


/*
 * Select a specific CPU or model.
 *
 * Called once to allow the back-end to set up the proper tables
 * and such, which may depend on the exact processor model used.
 */
int
set_cpu(const char *p, int pass)
{
    const target_t **t;

    if (opt_v && (pass == 1))
	printf("Setting processor to '%s'\n", p);

    target = NULL;

    for (t = targets; *t != NULL; t++) {
	if (! strcasecmp((*t)->name, p)) {
		target = *t;

		/* Create a predefined "CPU" symbol. */
		trg_symbol(target->name);

		return 1;
	}
    }

    return 0;
}


/* Create a target-specific symbol. */
void
trg_symbol(const char *name)
{
    char id[ID_LEN+1];
    value_t v = { 0 };
    int i = 0;

    id[i++] = 'P';
    do {
	id[i++] = (char)toupper(*name++);
	if (i >= ID_LEN)
		error(ERR_IDLEN, NULL);
    } while (*name != '\0');
    id[i] = '\0';

    v.v = 1;
    SET_DEFINED(v);
    SET_TYPE(v, TYPE_BYTE);

    define_variable(id, v);
}


/* Get a target-specific error message. */
const char *
trg_error(int err)
{
    if (target == NULL)
	error(ERR_NOCPU, NULL);

    return target->error(err);
}


/*
 * Process one instruction.
 *
 * This gets called by the main code, once per pass. Its task is
 * to process an entire instruction line, or, at least, the part
 * of the line where the assembler thinks the "code" is. So, in
 * most cases, we get called with "p" pointing to the very start
 * of the code and we can begin parsing right away.
 *
 * If we find an error, we simply call the error() function, and
 * that will properly terminate all processing for us.
 *
 * During pass 2, we will generate any code bytes, which we emit
 * by calling the emit() function.
 */
int
trg_instr(char **p, int pass)
{
    if (target == NULL)
	error(ERR_NOCPU, NULL);

    return target->instr(target, p, pass);
}


/*
 * Check to see if the identifier matches an existing mnemonic.
 *
 * The assembler will not allow the use of these "reserved words"
 * as label names.
 */
int
trg_instr_ok(const char *p)
{
    if (target == NULL)
	error(ERR_NOCPU, NULL);

    return target->instr_ok(target, p);
}
