/*
 * VASM		VARCem Multi-Target Macro Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B�ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle all functions.
 *
 * Version:	@(#)func.c	1.0.5	2023/08/20
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
#include "error.h"


typedef struct pseudo {
    const char	*name;
    value_t	(*func)(char **);
} func_t;


/* Implement the ".def(symbol)" function. */
static value_t
do_def(char **p)
{
    char id[ID_LEN];
    value_t res = { 0 };
    symbol_t *sym;

    ident(p, id);
    sym = sym_lookup(id, NULL);
    if (sym == NULL) {
	/* Forward reference, remember it. */
	sym = sym_aquire(id, NULL);
	if (DEFINED(sym->value))
		error(ERR_REDEF, id);

	/* Set the correct type. */
	sym->kind = KIND_VAR;
	sym->value.t = TYPE_BYTE;
	sym->value.v = 0;
    }
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    res = sym->value;

    return res;
}


/* Implement the "HI(symbol)" function. */
static value_t
do_high(char **p)
{
    value_t res;

    res = expr(p);
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    res.v = (res.v >> 8) & 0xff;
    res.t = TYPE_BYTE;
    SET_DEFINED(res);

    return res;
}


/* Implement the "LO(symbol)" function. */
static value_t
do_low(char **p)
{
    value_t res;

    res = expr(p);
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    res.v = (res.v & 0xff);
    res.t = TYPE_BYTE;
    SET_DEFINED(res);

    return res;
}


/* Implement the ".sum(startaddr,numbytes)" function. */
static value_t
do_sum(char **p)
{
    value_t res = { 0 };
    value_t v1, v2;

    v1 = expr(p);

    skip_white(p);
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    if (**p != ',')
	error(ERR_OPER, NULL);
    (*p)++;

    v2 = expr(p);

    /*
     * Calculate the (regular) checksum of the code bytes
     * between v1 (inclusive) and v2 (exclusive), using a
     * normal addition.
     */
    if (output_buff != NULL) {
	v2.v += v1.v;
	while (v1.v < v2.v)
		res.v += output_buff[v1.v++ - org];
    }
    SET_DEFINED(res);

    return res;
}


static const func_t functions[] = {
  { "DEF",	do_def		},
  { "DEFINED",	do_def		},
  { "H",	do_high		},
  { "HI",	do_high		},
  { "L",	do_low		},
  { "LO",	do_low		},
  { "SUM",	do_sum		},
  { NULL			}
};


value_t
function(const char *name, char **p)
{
    const func_t *ptr;
    value_t res = { 0 };
    int i;

    /* Skip any whitespace. */
    skip_white(p);

    for (ptr = functions; ptr->name != NULL; ptr++) {
	if ((i = strcmp(ptr->name, name)) >= 0) {
		if (i == 0)
			res = ptr->func(p);
		break;
	}
    }

    return res;
}
