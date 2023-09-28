/*
 * VASM		VARCem Multi-Target Macro Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle symbols.
 *
 * Version:	@(#)symbol.c	1.0.8	2023/09/26
 *
 * Authors:	Fred N. van Kempen, <waltje@varcem.com>
 *		Bernd B”ckmann, <https://codeberg.org/boeckmann/asm6502>
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
#include <string.h>
#include "global.h"
#include "error.h"


static symbol_t	*symbols = NULL;	// global symbol table


/* Create a new symbol, initialize to defaults. */
static symbol_t *
sym_new(const char *name)
{
    symbol_t *sym;

    sym = malloc(sizeof(symbol_t));
    if (sym == NULL)
	error(ERR_MEM, name);
    memset(sym, 0x00, sizeof(symbol_t));

    strcpy(sym->name, name);

    return sym;   
}


/* Return the first entry of the symbol table. */
symbol_t *
sym_table(void)
{
    return symbols;
}


/* Delete all entries from a symbol table. */
void
sym_free(symbol_t **table)
{
    symbol_t *sym, *next;

    if (table == NULL)
	table = &symbols;

    sym = *table;

    while (sym != NULL) {
	if (sym->locals != NULL)
		sym_free(&sym->locals);
	next = sym->next;
	free(sym);
	sym = next;
    }

    *table = NULL;
}


/* Look up a symbol in one of the tables. */
symbol_t *
sym_lookup(const char *name, symbol_t **table)
{
    symbol_t *ptr;
    int i;

    if (table == NULL)
	table = &symbols;

    for (ptr = *table; ptr != NULL; ptr = ptr->next) {
	if (opt_C)
		i = strcasecmp(name, ptr->name);
	else
		i = strcmp(name, ptr->name);

	if (! i)
		return ptr;
    }

    return NULL;
}


char
sym_type(const symbol_t *sym)
{
    switch (sym->kind) {
	case KIND_LBL:
		return 'L';

	case KIND_VAR:
		return 'V';

	case KIND_MAC:
		return 'M';
    }

    return '-';
}


symbol_t *
sym_aquire(const char *name, symbol_t **table)
{
    symbol_t *ptr, *sym;

    if (table == NULL)
	table = &symbols;

    sym = sym_lookup(name, table);

    if (sym == NULL) {
	sym = sym_new(name);

	/* Insert symbol in alphabetical order. */
	if ((*table == NULL) || (strcasecmp((*table)->name, name) > 0)) {
		sym->next = *table;
		*table = sym;
	} else {
		for (ptr = *table; ptr->next != NULL; ptr = ptr->next) {
			if (strcasecmp(ptr->next->name, name) > 0)
				break;
		}

		sym->next = ptr->next;
		ptr->next = sym;
	}
    }

    return sym;
}


symbol_t *
define_label(const char *id, uint32_t val, symbol_t *parent, int pass, int t)
{
    char nid[ID_LEN];
    symbol_t *sym;

    if (parent != NULL) {
	if (*id == DOT_CHAR) {
		/*
		 * This is a "dot label", meaning, its name
		 * has to be tacked on to the preceeding
		 * global label's name.
		 */
		if ((strlen(parent->name) + strlen(id)) >= ID_LEN)
			error(ERR_IDLEN, id);
		strcpy(nid, parent->name);
		strcat(nid, id);
		id = nid;
	}
	sym = sym_aquire(id, &parent->locals);
    } else
	sym = sym_aquire(id, NULL);

    if (pass == 1) {
	/* In pass 1, re-definitions are not allowed. */
	if (IS_VAR(sym) || (DEFINED(sym->value) && (sym->value.v != val)))
		error(parent ? ERR_LOCAL_REDEF : ERR_REDEF, id);
    } else {
	/*
	 * In pass 2, we ARE allowed to change a variable back to a label
	 * because this can happen in an EQU statement, for example.
	 */
#if 0
	if (DEFINED(sym->value) && (sym->value.v != val))
		error(parent ? ERR_LOCAL_REDEF : ERR_REDEF, id);
#endif
    }

    sym->kind = KIND_LBL;
    sym->subkind = t;
    sym->filenr = filenames_idx;
    sym->linenr = line;
    sym->value.v = val;
    sym->value.t = ((TYPE(sym->value) == TYPE_WORD)
			? TYPE_WORD : NUM_TYPE(val)) | VALUE_DEFINED;

    return sym;
}


void
define_variable(const char *id, value_t v, int force)
{
    symbol_t *sym;

    sym = sym_aquire(id, NULL);
    if (!force && DEFINED(sym->value) && (sym->value.v != v.v))
	error(ERR_REDEF, id);

    sym->kind = KIND_VAR;
    sym->filenr = filenames_idx;
    sym->linenr = line;

    /* if the type is already set do not change it */
    sym->value.v = v.v;
    if (!force && TYPE(sym->value)) {
#if 0
	if (NUM_TYPE(v.v) > TYPE(sym->value)) error(ERR_REDEF, id);
#endif
	if (DEFINED(v))
		SET_DEFINED(sym->value);
    } else
	sym->value.t = v.t;
}


const char *
sym_print(const symbol_t *sym)
{
    static char buff[32];

    switch (TYPE(sym->value)) {
	case TYPE_BYTE:
		sprintf(buff, "%02X", sym->value.v & 0xff);
		break;

	case TYPE_WORD:
		sprintf(buff, "%04X", sym->value.v & 0xffff);
		break;

	case TYPE_DWORD:
		sprintf(buff, "%08X", sym->value.v);
		break;
    }

    return buff;
}
