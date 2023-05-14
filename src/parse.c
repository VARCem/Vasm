/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B�ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Parse the source input, process it, and generate output.
 *
 * Version:	@(#)parse.c	1.0.8	2023/05/13
 *
 * Authors:	Fred N. van Kempen, <waltje@varcem.com>
 *		Bernd B�ckmann, <https://codeberg.org/boeckmann/asm6502>
 *
 *		Copyright 2023 Fred N. van Kempen.
 *		Copyright 2022,2023 Bernd B�ckmann.
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
#include <ctype.h>
#include <setjmp.h>
#include "global.h"
#define HAVE_SETJMP_H
#include "error.h"
#include "target.h"


int		line,			// currently processed line number
		newline,		// next line to be processed
		found_end;		// END directive was found
symbol_t	*current_label = NULL;	// search scope for local labels
int8_t		org_done;		// has a .org been performed?
int8_t		radix;			// current numerical radix
int		iflevel,		// current level of conditionals
		ifstate, newifstate,	// current conditional state
		ifstack[MAX_IFLEVEL];
int		rptlevel,
		rptstate, newrptstate;
repeat_t	rptstack[MAX_RPTLEVEL];

/* program counter and output counter may not be in sync */
/* this happens if an .org directive is used, which modifies the */
/* program counter but not the output counter. */
uint16_t	pc = 0;			// addr of currently assembled instr
uint16_t	oc = 0;			// counter of emitted output bytes
uint16_t	sa = 0;			// start addr for generated code (.end)


#ifdef _DEBUG
char *
dumpline(const char *p)
{
    static char temp[1024];
    char *s = temp;

    while (*p && *p != '\n')
	*s++ = *p++;
    *s = '\0';

    return(temp);
}
#endif


void
skip_eol(char **p)
{
    if (**p == EOF_CHAR)
	(*p)++;
    if (**p == 0x0d)
	(*p)++;
    if (**p == 0x0a)
	(*p)++;
}


int
is_end(char p)
{
    return IS_END(p);
}


void
skip_white(char **p)
{
    while (IS_SPACE(**p))
	(*p)++;
}


void
skip_white_and_comment(char **p)
{
    while (IS_SPACE(**p))
	(*p)++;

    if (**p == COMMENT_CHAR) {
	(*p)++;

	while (! IS_END(**p))
		(*p)++;
    }
}


void
skip_curr_and_white(char **p)
{
    (*p)++;

    while (IS_SPACE(**p))
	(*p)++;
}


static void
_ident(char **p, char *id, int numeric)
{
    int i = 0;

    /* Identifiers may have letters, digits, and some specials. */
    if ((!numeric && !isalpha(**p) && !IS_IDENT(**p)) ||
	(!isalnum(**p) && !IS_IDENT(**p))) error(ERR_ID, NULL);

    do {
	*id++ = *(*p)++;
	if (++i >= ID_LEN)
		error(ERR_IDLEN, NULL);
    } while (isalnum(**p) || IS_IDENT(**p));

    *id = '\0';
}


/* Read identifier which may not start with a digit. */
void
ident(char **p, char *id)
{
    _ident(p, id, 0);
}


/* Read identifier which may start with a digit. */
void
nident(char **p, char *id)
{
    _ident(p, id, 1);
}


/* Read string and convert to uppercase. */
void
upcase(char **p, char *str)
{
    int i = 0;

    do {
	*str++ = (char)toupper(*(*p)++);
	if (++i >= ID_LEN)
		error(ERR_IDLEN, NULL);
    } while (isalnum(**p));

    *str = '\0';
}


/* Read identifier and convert to uppercase. */
void
ident_upcase(char **p, char *id)
{
    int i = 0;

    if (!isalpha(**p) && !IS_IDENT(**p))
	error(ERR_ID, NULL);

    do {
	*id++ = (char)toupper(*(*p)++);
	if (++i >= ID_LEN)
		error(ERR_IDLEN, NULL);
    } while (isalnum(**p) || IS_IDENT(**p));

    *id = '\0';
}


/* Read identifier and convert to uppercase. */
void
nident_upcase(char **p, char *id)
{
    int i = 0;

    do {
	*id++ = (char)toupper(*(*p)++);
	if (++i >= ID_LEN)
		error(ERR_IDLEN, NULL);
    } while (isalnum(**p) || IS_IDENT(**p));

    *id = '\0';
}


/* Processes one statement or assembler instruction. */
static char *
statement(char **p, int pass)
{
    char id[ID_LEN], id2[ID_LEN];
    const struct pseudo *ptr;
    char *newp = NULL;
    value_t v;
    char *pt;

    skip_white_and_comment(p);
    if (IS_END(**p))
	return NULL;

    /* First check for variable or label definition. */
    pt = *p;
    if (isalpha(**p)) {
	ident(p, id);
	skip_white(p);

	if (**p == EQUAL_CHAR) {
		(*p)++;

		v = expr(p);

		if (ifstate)
			define_variable(id, v, 0);
		return NULL;
	} else if ((**p == COLON_CHAR) || !trg_instr_ok(id)) {
		if ((ptr = is_pseudo(id, 0)) != NULL) {
			/* All good, we're a pseudo-op. */
			newp = pseudo(ptr, p, pass);
			return newp;
		}

		/*
		 * Labels either have a colon at the end, OR are anything
		 * but an valid instruction. To force a label with the same
		 * name as an instruction, add a colon to it :)
		 */
		if (**p == COLON_CHAR)
			(*p)++;

		if (ifstate)
			current_label = define_label(id, pc, NULL, 1);

		skip_white_and_comment(p);
		if (IS_END(**p))
			return NULL;
	} else
		*p = pt;
    }

    /* Local label definition? */
    else if (**p == ALPHA_CHAR) {
	(*p)++;
	nident(p, id);

	if (ifstate) {
		if (current_label == NULL)
			error(ERR_NO_GLOBAL, NULL);

		define_label(id, pc, current_label, 0);
	}

	skip_white(p);

	if (**p == COLON_CHAR)
		(*p)++;

	skip_white_and_comment(p);
	if (IS_END(**p))
		return NULL;
    }

    /* Check for directive or instruction. */
again:
    if (**p == DOT_CHAR) {
	/* Local label or directive. */
	pt = *p;
	(*p)++;
	nident_upcase(p, id);
	if ((ptr = is_pseudo(id, 1)) != NULL) {
		/* All good, we're a directive. */
		newp = pseudo(ptr, p, pass);

		return newp;
	}

	if (ifstate) {
		if (current_label == NULL)
			error(ERR_NO_GLOBAL, NULL);

		/* Restore our pointer and get the label ID. */
		*p = pt;
		nident(p, id2);

		/* We don't care about the colon. */
		if (**p == COLON_CHAR)
			(*p)++;

		if ((strlen(current_label->name) + strlen(id2)) >= ID_LEN)
			error(ERR_IDLEN, id2);
		strcpy(id, current_label->name);
		strcat(id, id2);

		define_label(id, pc, NULL, 0);
	}

	skip_white_and_comment(p);
	if (IS_END(**p))
		return NULL;

	/* Restart scanning for statements on this line. */
	goto again;
    }

    /* Check if this is a pseudo (directive without the dot.) */
    pt = *p;
    nident_upcase(p, id);
    if ((ptr = is_pseudo(id, 0)) != NULL) {
	/* All good, we're a directive. */
	skip_white(p);
	newp = pseudo(ptr, p, pass);

	return newp;
    }
    *p = pt;

    skip_white_and_comment(p);
    if (IS_END(**p))
	return NULL;

    /* We only parse the rest if we are active. */
    if (ifstate) {
	if (isalpha(**p)) {
		/* Execute instruction and update program counter. */
		pc += trg_instr(p, pass);
	} else
		error(ERR_STMT, NULL);
    } else {
	/* Just skip. */
	while (! IS_END(**p))
		(*p)++;
    }

    return NULL;
}


int
pass(char **p, int pass)
{
    const char *msg;
    char *list;
    char *newp;
    int err;

    errors = 0;
    found_end = 0;
    line = 1;
    current_label = NULL;
    org_done = 0;
    radix = RADIX_DEFAULT;
    pc = oc = 0;
    filenames_idx = 0;
    iflevel = 0;
    ifstate = 1;
    memset(ifstack, 0x00, sizeof(ifstack));
    rptlevel = 0;
    rptstate = 0;
    memset(rptstack, 0x00, sizeof(rptstack));

    list_set_head(NULL);

    if ((err = setjmp(error_jmp)) == 0) {
	while (p && **p) {
		/* Save current line position for the listing. */
		list = *p;

		if (! rptstate)
			newline = line + 1;

		newifstate = ifstate;
		newrptstate = rptstate;

		newp = statement(p, pass);

		skip_white_and_comment(p);

		/*
		 * Every statement ends with a newline.
		 * If it is not found here it is an error condition.
		 */
		if (! IS_END(**p))
			error(ERR_EOL, NULL);

		skip_eol(p);

		if ((pass == 2) && ((rptlevel == 0) || rptstate))
			list_line(list);

		line = newline;
		ifstate = newifstate;
		rptstate = newrptstate;

		if (**p == EOF_CHAR) {
			(*p)++;
next_file:
			filenames_idx++;
			line = filelines[filenames_idx];
		}

		if (found_end) {
			found_end = 0;

			/* If we are in an included file, go down a level. */
			if (filenames_idx > 0)
				goto next_file;

			/* Otherwise, force to be done. */
			p = NULL;
		}

		if (newp != NULL)
			text = newp;

		list_save(pc, oc);
	}

	/* Make sure we have matched IF..ENDIF at the end. */
	if (iflevel > 0)
		error(ERR_ENDIF, "** end of input**");

	/* Make sure we have matched REPEAT..ENDREP at the end. */
	if (rptlevel > 0)
		error(ERR_ENDREP, "** end of input**");
    } else {
	if (err < ERR_MAXERR)
		msg = err_msgs[err];
	else
		msg = trg_error(err);

	printf("%s:%i: error: %s",
		filenames[filenames_idx], line, msg);
	if (error_hint[0] != '\0')
		printf(" (%s)", error_hint);
	printf("\n");
    }

    return errors;
}
