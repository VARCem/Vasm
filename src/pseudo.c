/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle directives and pseudo-ops.
 *
 * Version:	@(#)pseudo.c	1.0.8	2023/06/16
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
#include <ctype.h>
#include "global.h"
#include "error.h"


typedef struct pseudo {
    const char	*name;
    int		always;
    int		dotted;
    char	*(*func)(char **, int);
    char	*(*list)(char *);
} pseudo_t;


static int
string_lit(char **p, char *buf, int bufsize, int quot)
{
    const char *start = buf;
    int needquot = quot;

    if (**p != '"') {
	if (quot)
		error(ERR_STR, NULL);
    } else
 	(*p)++;

    while (! IS_END(**p)) {
	if (**p == '"')
		break;

	if ((buf-start) >= (bufsize-1))
		error(ERR_STRLEN, NULL);

	*(buf++) = **p;
	(*p)++;
    }
    *buf = '\0';
   
    if (IS_END(**p)) {
	if (needquot)
		error(ERR_STREND, NULL);
    } else
	(*p)++;

    return (int)(buf - start);
}


/* The ".align [<count>]" directive. */
static char *
do_align(char **p, int pass)
{
    int count = 2;
    value_t v;

    skip_white(p);
    if (! IS_END(**p)) {
	/* We were given an alignment factor. */
	v = expr(p);
 	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	count = v.v;
    }

    if ((count != 1) && (count != 2) && (count != 4) && (count != 8))
	error(ERR_RNG, NULL);

    /* Now "fill out" the space with bytes. */
    count--;
    while ((pc & count) != 0) {
	emit_byte(0x00, pass);

	pc++;
    };

    return NULL;
}


/* The ".asciz "string"[,"string",...]" directive. */
static char *
do_asciz(char **p, int pass)
{
    char buf[STR_LEN];
    int next, len;

    do {
	next = 0;

	skip_white(p);
	if (**p == '"') {
		len = string_lit(p, buf, STR_LEN, 1);
		emit_str(buf, len, pass);
		pc += len;
		emit_byte(0x00, pass);
		pc++;
	} else
		error(ERR_STR, NULL);

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    return NULL;
}


/* The ".assert <expr>" directive. */
static char *
do_assert(char **p, int pass)
{
    value_t v;

    skip_white_and_comment(p);
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    v = expr(p);
    if (UNDEFINED(v) || v.v == 0)
	error(ERR_ASSERT, NULL);

    return NULL;
}


/* The ".blob <filename>[,[skip][,count]]" directive. */
static char *
do_blob(char **p, int pass)
{
    char filename[STR_LEN];
    size_t count, skip;
    value_t v;
    FILE *fp;
    int b;

    /* Read filename. */
    skip_white(p);
    string_lit(p, filename, STR_LEN, 1);
    skip_white_and_comment(p);

    /* Check for more arguments. */
    count = skip = 0;
    if (**p == ',') {
	skip_curr_and_white(p);
	if (!IS_END(**p) && (**p != ',')) {
		/* Optional "skip" count Blank means 0. */
		v = expr(p);
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);
		skip = (size_t)v.v;
	}

	skip_white(p);
	if (**p == ',') {
		/* If comma present, "count" must be present. */
		skip_curr_and_white(p);
		v = expr(p);
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);
		count = (size_t)v.v;
	}
    }

    /* Make sure we can open the file.. */
    fp = fopen(filename, "rb");
    if (fp == NULL)
	error(ERR_OPEN, filename);

    /*
     * Read data from file, and "insert" the bytes into
     * source as if they had been .byte statements.
     */
    while (!feof(fp) && !ferror(fp)) {
	b = fgetc(fp);
	if (b == EOF)
		break;

	if (skip > 0)
		skip--;
	else {
		emit_byte((uint8_t)b, pass);
		pc++;

		if ((count > 0) && (--count == 0))
			break;
	}
    }

    (void)fclose(fp);

    return NULL;
}


/* The ".byte <data>[,<data>,...]" directive. */
static char *
do_byte(char **p, int pass)
{
    char buf[STR_LEN];
    value_t v;
    int next, len;

    do {
	next = 0;

	skip_white(p);
	if (**p == '"') {
		len = string_lit(p, buf, STR_LEN, 1);
		emit_str(buf, len, pass);
		pc += len;
	} else if (**p == '\'') {
		(*p)++;
		if (**p == '\'')
			error(ERR_CHR, NULL);

		buf[0] = **p;

		(*p)++;
		if (**p != '\'')
			error(ERR_CHREND, NULL);
		len = 1;
		emit_str(buf, len, pass);

		(*p)++;
		pc++;
	} else {
		v = expr(p);

		if (pass == 2) {
			if (UNDEFINED(v))
				error(ERR_UNDEF, NULL);
			if ((TYPE(v) != TYPE_BYTE) && (v.v > 0xff))
				error(ERR_ILLTYPE, NULL);
		}
		emit_byte((uint8_t)to_byte(v, 0).v, pass);

		pc++;
	}

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    return NULL;
}


/* The ".cpu <processor_name>" directive. */
static char *
do_cpu(char **p, int pass)
{
    char name[ID_LEN];

    skip_white_and_comment(p);
    if (IS_END(**p))
	error(ERR_ID, NULL);

    nident_upcase(p, name);

    if (! set_cpu(name, pass))
	error(ERR_CPU, name);

    return NULL;
}


/* Define a symbol from the command line. */
static char *
do_define(char **p, int pass)
{
    char id[ID_LEN];
    value_t v = { 0 };

    skip_white(p);

    ident(p, id);
    if (**p == EQUAL_CHAR) {
	/* We have an equal sign. */
	(*p)++;
	if (! IS_END(**p)) {
		/* Try to decode a value. */
		v = expr(p);
	} else {
		/* No value given, whoops. */
		goto nodata;
	}
    } else {
nodata:
	v.v = 1;
	SET_DEFINED(v);

    }

    define_variable(id, v, 0);

    return NULL;
}


/* List the results of a .define directive. */
static char *
do_define_list(char *str)
{
    symbol_t *sym = sym_lookup(str, NULL);

    sprintf(str, "= %s", sym_print(sym));

    return str;
}


/* The ".dword <data>[,<data>,...]" directive. */
static char *
do_dword(char **p, int pass)
{
    value_t v;
    int next;

    do {
	next = 0;

	skip_white(p);

	v = expr(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	emit_dword(v.v, pass);
	pc += 4;

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    return NULL;
}


/* The ".echo [<expr>[,<expr>,...]]" directive. */
static char *
do_echo(char **p, int pass)
{
    char temp[STR_LEN];
    int fmt, next;
    value_t v;

    if (pass == 2) {
	while (! IS_END(**p))
		(*p)++;
	return NULL;
    }

    do {
	next = 0;

	skip_white(p);
	if (**p == '"') {
		string_lit(p, temp, STR_LEN, 1);
		printf("%s", temp);
	} else {
		fmt = value_format(p);
		if (fmt == 0)
			fmt = FMT_DEC_CHAR;
		else if (fmt < 0)
			error(-fmt, NULL);

		v = expr(p);
		if (DEFINED(v))
			printf("%s", value_print_format(v, fmt));
		else
			printf("??");
	}

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    putchar('\n');

    return NULL;
}


/* The ".else" directive. */
static char *
do_else(char **p, int pass)
{
    skip_white_and_comment(p);
    if (! IS_END(**p))
	error(ERR_EOL, NULL);

    if (iflevel > 0)
	newifstate = !ifstate;
    else
	error(ERR_ELSE, NULL);

    return NULL;
}


/* The ".end [<start_address>]" directive. */
static char *
do_end(char **p, int pass)
{
    value_t v;

    skip_white(p);
    if (! IS_END(**p)) {
	/* Get the value for the start address. */
	v = expr(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	/* Set the start address. */
	sa = v.v;

	output_start(sa, pass);
    }

    /*
     * Historically, this was to signal an assembler to stop parsing
     * input and just read until end of tape (so the tape reel or card
     * deck could be removed.) We do similar, we just "eat" any more
     * input until we hit the end of this "file".
     */
    do {
	if (**p)
		(*p)++;
    } while (**p && **p != EOF_CHAR);

    found_end = 1;

    return NULL;
}


/* List the results of a .end directive. */
static char *
do_end_list(char *str)
{
    sprintf(str, "$= %06X", sa);

    return str;
}


/* The ".endif" directive. */
static char *
do_endif(char **p, int pass)
{
    skip_white_and_comment(p);
    if (! IS_END(**p))
	error(ERR_EOL, NULL);

    if (iflevel > 0) {
	newifstate = ifstack[--iflevel];
	ifstate = ifstack[iflevel];
    } else
	error(ERR_ENDIF, NULL);

    return NULL;
}


/* Perform the ".endrep" directive. */
static char *
do_endrep(char **p, int pass)
{
    if (rptlevel == 0 || rptstack[rptlevel - 1].file != filenames_idx)
	error(ERR_REPEAT, NULL);

    if (rptstack[rptlevel - 1].count > 1) {
	*p = rptstack[rptlevel - 1].pos;
	line = rptstack[rptlevel - 1].line;
	rptstack[rptlevel - 1].count--;
	rptstack[rptlevel - 1].repeating = 1;
    } else
	rptlevel--;

    rptstate = 0;
    newrptstate = (rptstack[rptlevel - 1].count > 0);

    return NULL;
}


/* The ".error <text>" directive. */
static char *
do_error(char **p, int pass)
{
    char buff[4*STR_LEN], *s;
    char temp[STR_LEN];
    int fmt, next;
    value_t v;

    s = buff;

    do {
	next = 0;

	skip_white(p);
	if (**p == '"') {
		string_lit(p, temp, STR_LEN, 1);
		sprintf(s, "%s", temp);
		s += strlen(s);
	} else {
		fmt = value_format(p);
		if (fmt == 0)
			fmt = FMT_DEC_CHAR;
		else if (fmt < 0)
			error(-fmt, NULL);

		v = expr(p);
		if (DEFINED(v))
			sprintf(s, "%s", value_print_format(v, fmt));
		else
			sprintf(s, "??");
		s += strlen(s);
	}

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    error(ERR_USER, buff);
    /*NOTREACHED*/

    return NULL;
}


/* The ".equ <expr>" directive. */
static char *
do_equ(char **p, int pass)
{
    value_t v;

    if (current_label == NULL)
	error(ERR_LABEL, NULL);

    v = expr(p);

    /* (Re-)define this label as a variable. */
    define_variable(current_label->name, v, 1);

    return NULL;
}


/* List the results of a .equ directive. */
static char *
do_equ_list(char *str)
{
    sprintf(str, "= %s", sym_print(current_label));

    return str;
}


/* The ".fill <count> [,<data>]" directive. */
static char *
do_fill(char **p, int pass)
{
    uint8_t filler = 0x00;
    value_t v;
    int count;

    /* Get the size of the fill block. */
    v = expr(p);
    if ((pass == 2) && UNDEFINED(v))
	error(ERR_UNDEF, NULL);
    count = v.v;

    skip_white(p);
    if (**p == ',') {
	skip_curr_and_white(p);

	/* Get the optional filler byte value (default $FF.) */
	v = expr(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);
	if (TYPE(v) != TYPE_BYTE)
		error(ERR_ILLTYPE, NULL);
	filler = (uint8_t)v.v;
    }

    /* Now "fill out" the space with bytes. */
    while (count--) {
	emit_byte(filler, pass);

	pc++;
    };

    return NULL;
}


/* The ".if <expr>" directive. */
static char *
do_if(char **p, int pass)
{
    value_t v;

    skip_white(p);

    /* Get the value of the expression. */
    v = expr(p);

    /*
     * It may have been that one of the symbols
     * referenced was not defined. We can make
     * this an error, or simply "assume" the
     * value to be 0.
     */
    if ((pass == 2) && UNDEFINED(v)) {
#ifndef ALLOW_UNDEFINED_IF
	error(ERR_UNDEF, NULL);
#endif
    }

    if (iflevel < MAX_IFLEVEL) {
	ifstack[iflevel++] = ifstate;
	newifstate = !!v.v;
    } else
	error(ERR_IF, NULL);

    /* If we are skipping, keep skipping! */
    if (! ifstate)
	newifstate = ifstate;

    return NULL;
}


/* The ".ifdef <name>" directive. */
static char *
do_ifdef(char **p, int pass)
{
    char id[ID_LEN];
    symbol_t *sym;

    skip_white(p);

    nident(p, id);
    sym = sym_lookup(id, NULL);
    if (sym != NULL && sym->kind != KIND_VAR)
	sym = NULL;

    if (iflevel < MAX_IFLEVEL) {
	ifstack[iflevel++] = ifstate;
	newifstate = ((sym != NULL) && DEFINED(sym->value)) ? 1 : 0;
    } else
	error(ERR_IF, NULL);

    /* If we are skipping, keep skipping! */
    if (! ifstate)
	newifstate = ifstate;

    return NULL;
}


/* The ".ifn <expr>" directive. */
static char *
do_ifn(char **p, int pass)
{
    value_t v;

    skip_white(p);

    /* Get the value of the expression. */
    v = expr(p);

    /*
     * It may have been that one of the symbols
     * referenced was not defined. We can make
     * this an error, or simply "assume" the
     * value to be 0.
     */
    if ((pass == 2) && UNDEFINED(v)) {
#ifndef ALLOW_UNDEFINED_IF
	error(ERR_UNDEF, NULL);
#endif
    }

    if (iflevel < MAX_IFLEVEL) {
	ifstack[iflevel++] = ifstate;
	newifstate = !!!v.v;
    } else
	error(ERR_IF, NULL);

    /* If we are skipping, keep skipping! */
    if (! ifstate)
	newifstate = ifstate;

    return NULL;
}


/* The ".ifndef <name>" directive. */
static char *
do_ifndef(char **p, int pass)
{
    char id[ID_LEN];
    symbol_t *sym;

    skip_white(p);

    nident(p, id);
    sym = sym_lookup(id, NULL);
    if (sym != NULL && sym->kind != KIND_VAR)
	sym = NULL;

//printf(">> IFNDEF(%d) pass=%d state=%d\n", iflevel, pass, ifstate);
    if (iflevel < MAX_IFLEVEL) {
	ifstack[iflevel++] = ifstate;
	newifstate = ((sym != NULL) && DEFINED(sym->value)) ? 0 : 1;

	/*
	 * HACK.
	 *
	 * We are testing for the presence of a defined variable,
	 * which may or may not exist. If our source code first
	 * tests for this variable, and then DOES create or delete
	 * in, things get messed up.
	 *
	 * Case in point:
	 *
	 *  .ifndef FOOBAR
	 *    FOOBAR = 1234
	 *  .endif
	 *
	 * In pass 1, FOOBAR does not exist yet, so we will happily
	 * set IFSTATE here, and execute the enclosed = line, which
	 * will show up in the listing.
	 *
	 * In pass 2, however, this changes: the same variable now
	 * DOES exist (as we just created it..), and so the .ifndef
	 * fails - the = line will not be executed.
	 *
	 * This is wrong. Although the variable still exists (it will
	 * be re-defined to the same value and type in pass 2, which
	 * is OK), it shows up as "not executed"...
	 *
	 * So, for the sake of just this, we save the "state" as we
	 * found it in pass 1 into the variable's definition so it
	 * can be checked against in pass 2 ...
	 */
//printf(">>> pass=%d ifstate=%d\n", pass, newifstate);
	if (pass == 1) {
		/* Save state. */
		if (sym != NULL)
			sym->pass = newifstate;
	} else {
		/* Pass 2, check state from pass 1. */
		if (sym != NULL)
			newifstate = sym->pass;
	}
//printf("NEWstate = %d\n", newifstate);
    } else
	error(ERR_IF, NULL);

    /* If we are skipping, keep skipping! */
    if (! ifstate)
	newifstate = ifstate;

    return NULL;
}


/* The ".include <filename>" directive. */
static char *
do_include(char **p, int pass)
{
    char filename[STR_LEN];
    char *ntext = NULL;
    size_t last_sz, last_off;
    size_t pos, size;
    int i;

    /* Do we have room for another include file? */
    if (filenames_len + 2 > MAX_FILENAMES)
	error(ERR_MAXINC, NULL);

    /* Read filename. */
    skip_white(p);
    string_lit(p, filename, STR_LEN, 1);
    skip_white_and_comment(p);
    if (! IS_END(**p))
	error(ERR_EOL, NULL);
    skip_eol(p);

    ntext = *p;

    if (pass == 1) {
	/* Point at the first character of the line following the directive. */
	last_off = (int)(*p - text);
	last_sz = text_len - last_off;

	size = file_size(filename);
   
	/* Calculate new source length and aquire memory. */
	text_len = last_off + 1 + size + 1 + last_sz;
	ntext = malloc(text_len + 1);	// plus NUL at end
	if (ntext == NULL)
		error(ERR_MEM, NULL);

	/* Copy pre-include block into buffer and terminate it. */
	memcpy(ntext, text, last_off);
	ntext[last_off++] = EOF_CHAR;

	/* Now read the new file into the buffer and terminate it. */
	pos = last_off;
	if (file_read_buf(filename, ntext + last_off) == 0)
		error(ERR_OPEN, filename);
	last_off += size;
	ntext[last_off++] = EOF_CHAR;

	/* Finally, move second block of original buffer and terminate it. */
	memcpy(ntext + last_off, *p, last_sz);
	last_off += last_sz;

	ntext[last_off] = '\0';

	/* Set source pointer to beginning of included file. */
	*p = ntext + pos;
    }

    /* Break up current file and make spaces for two new files. */
    for (i = (filenames_len - 1); i > (filenames_idx + 1); i--) {
	filenames[i] = filenames[i - 2];
	filelines[i] = filelines[i - 2];
    }
    filenames_len += 2;
    filenames[filenames_idx + 2] = filenames[filenames_idx];
    filelines[filenames_idx + 2] = line + 1;

    /* Add this included file in the middle. */
    filenames[filenames_idx + 1] = strdup(filename);
    filelines[filenames_idx + 1] = 1;

    /* We are now "in" the included file. */
    filenames_idx++;
    newline = filelines[filenames_idx];

    return ntext;
}


/* The ".nofill" directive. */
static char *
do_nofill(char **p, int pass)
{
    opt_F = 0;

    return NULL;
}


/* The ".org <address>" directive. */
static char *
do_org(char **p, int pass)
{
    value_t v;

    skip_white(p);

    /* Get the new value for the origin. */
    v = expr(p);
    if ((pass == 2) && UNDEFINED(v))
	error(ERR_UNDEF, NULL);

    /* Set the new origin. */
    org = pc = v.v;

    /* Remember our load address. */
    output_addr(pc, pass);

    return NULL;
}


/* List the results of a .org directive. */
static char *
do_org_list(char *str)
{
    sprintf(str, "*= %06X", pc);

    return str;
}


/* The ".page [<width][,<height>]" directive. */
static char *
do_page(char **p, int pass)
{
    value_t v;

    skip_white_and_comment(p);
    if (IS_END(**p)) {
	/* This was just a .page to start a new page. */
	list_page(NULL, NULL);
	return NULL;
    }

    /* Get the number of lines per page. */
    if (**p != ',') {
	v = expr(p);
	skip_white(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	/* Set the new number of lines per page. */
	list_plength = (int)v.v;
    }

    if (**p == ',') {
	/* Comma present, so also have line length. */
	skip_curr_and_white(p);

	/* Get the number of characters per line. */
	v = expr(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	/* Set the new number of characters per line. */
	list_pwidth = (int)v.v;
    }

    return NULL;
}


/* The ".radix [2|8|10|16]" directive. */
static char *
do_radix(char **p, int pass)
{
    value_t v;

    skip_white_and_comment(p);
    if (IS_END(**p)) {
	/* Terminate current radix, reset to default. */
	radix = RADIX_DEFAULT;
	if (opt_v && pass == 1)
		printf("Resetting radix to %i\n", radix);

	return NULL;
    }

    /*
     * We may currently be in a non-decimal radix, and if we
     * then get a line saying
     *
     * .radix 16
     *
     * this could be an invalid number for that radix.  So,
     * we first reset it to the default before trying to
     * read the new one.
     */
    radix = RADIX_DEFAULT;

    skip_white(p);
    v = expr(p);
    if ((pass == 2) && UNDEFINED(v))
	error(ERR_UNDEF, NULL);

    if (v.v != 2 && v.v != 8 && v.v != 10 && v.v != 16)
	error(ERR_RNG, NULL);

    radix = (int8_t)v.v;
    if (opt_v && pass == 1)
	printf("Setting radix to %i\n", radix);

    return NULL;
}


/* Perform the ".repeat expr" directive. */
static char *
do_repeat(char **p, int pass)
{
    value_t v;
    char *pt;

    if (rptlevel == MAX_RPTLEVEL)
	error(ERR_MAX_REP, NULL);

    skip_white(p);
    v = expr(p);
    if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);

    /* Find next line to continue by ENDREP. */
    pt = *p;
    skip_white_and_comment(p);

    rptstack[rptlevel].repeating = 0;
    rptstack[rptlevel].count = v.v;
    rptstack[rptlevel].line = line + 1;
    rptstack[rptlevel].pos = *p;
    rptstack[rptlevel].file = filenames_idx;

    newrptstate = (rptstack[rptlevel].count > 0);
    rptstate = newrptstate;
    rptlevel++;

    *p = pt;

    return NULL;
}


/* The ".subttl <text>" directive. */
static char *
do_subttl(char **p, int pass)
{
    char buff[STR_LEN];

    /* Read filename. */
    skip_white(p);
    string_lit(p, buff, STR_LEN, 0);

    /* Free any current sub-heading. */
    list_set_head_sub(buff);

    return NULL;
}


/* The ".syms [off|on|full]" directive. */
static char *
do_syms(char **p, int pass)
{
    char id[ID_LEN];
    int syms = 0;

    skip_white_and_comment(p);
    if (! IS_END(**p)) {
	nident_upcase(p, id);

	if (! strcmp(id, "OFF"))
		syms = 0;
	else if (! strcmp(id, "ON"))
		syms = 1;
	else if (! strcmp(id, "FULL"))
		syms = 2;
	else
		error(ERR_STR, NULL);
    }

    list_set_syms(syms);

    return NULL;
}


/* The ".title <text>" directive. */
static char *
do_title(char **p, int pass)
{
    char buff[STR_LEN];

    /* Read filename. */
    skip_white(p);
    string_lit(p, buff, STR_LEN, 0);

    /* Free any current heading. */
    list_set_head(buff);

    return NULL;
}


/* The ".warn <text>" directive. */
static char *
do_warn(char **p, int pass)
{
    char buff[4*STR_LEN], *s;
    char temp[STR_LEN];
    int fmt, next;
    value_t v;

    s = buff;
    sprintf(s, "*** WARNING: ");
    s += strlen(s);

    do {
	next = 0;

	skip_white(p);
	if (**p == '"') {
		string_lit(p, temp, STR_LEN, 1);
		sprintf(s, "%s", temp);
		s += strlen(s);
	} else {
		fmt = value_format(p);
		if (fmt == 0)
			fmt = FMT_DEC_CHAR;
		else if (fmt < 0)
			error(-fmt, NULL);

		v = expr(p);
		if (DEFINED(v))
			sprintf(s, "%s", value_print_format(v, fmt));
		else
			sprintf(s, "??");
		s += strlen(s);
	}

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    printf("%s\n", buff);

    return NULL;
}


/* The ".width <width>" directive. */
static char *
do_width(char **p, int pass)
{
    value_t v;

    skip_white_and_comment(p);
    if (IS_EOL(**p))
	error(ERR_EOL, NULL);

    /* Get the number of characters per line. */
    v = expr(p);
    if ((pass == 2) && UNDEFINED(v))
	error(ERR_UNDEF, NULL);

    /* Set the new number of characters per line. */
    list_pwidth = (int)v.v;

    return NULL;
}


/* The ".word <data>[,<data>,...]" directive. */
static char *
do_word(char **p, int pass)
{
    value_t v;
    int next;

    do {
	next = 0;
	skip_white(p);

	v = expr(p);
	if ((pass == 2) && UNDEFINED(v))
		error(ERR_UNDEF, NULL);
	emit_word(v.v & 0xffff, pass);
	pc += 2;

	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    return NULL;
}


static const pseudo_t pseudos[] = {
  { "ALIGN",	0, 0, do_align,		NULL		},
  { "ASCII",	0, 1, do_byte,		NULL		},
  { "ASCIIZ",	0, 1, do_asciz,		NULL		},
  { "ASCIZ",	0, 1, do_asciz,		NULL		},
  { "ASSERT",	0, 1, do_assert,	NULL		},
  { "BINARY",	0, 1, do_blob,		NULL		},
  { "BLOB",	0, 1, do_blob,		NULL		},
  { "BYTE",	0, 0, do_byte,		NULL		},
  { "CPU",	0, 1, do_cpu,		NULL		},
  { "DATA",	0, 1, do_byte,		NULL		},
  { "DB",	0, 0, do_byte,		NULL		},
  { "DEFINE",	0, 0, do_define,	do_define_list	},
  { "DL",	0, 0, do_dword,		NULL		},
  { "DS",	0, 0, do_fill,		NULL		},
  { "DW",	0, 0, do_word,		NULL		},
  { "DWORD",	0, 0, do_dword,		NULL		},
  { "ECHO",	0, 1, do_echo,		NULL		},
  { "ELSE",	1, 0, do_else,		NULL		},
  { "END",	0, 0, do_end,		do_end_list	},
  { "ENDIF",	1, 0, do_endif,		NULL		},
  { "ENDREP",	0, 0, do_endrep,	NULL		},
  { "EQU",	0, 0, do_equ,		do_equ_list	},
  { "ERROR",	0, 1, do_error,		NULL		},
  { "FI",	1, 0, do_endif,		NULL		},
  { "FILL",	0, 1, do_fill,		NULL		},
  { "IF",	1, 0, do_if,		NULL		},
  { "IFDEF",	1, 0, do_ifdef,		NULL		},
  { "IFN",	1, 0, do_ifn,		NULL		},
  { "IFNDEF",	1, 0, do_ifndef,	NULL		},
  { "INCLUDE",	0, 0, do_include,	NULL		},
  { "NOFILL",	0, 0, do_nofill,	NULL		},
  { "ORG",	0, 0, do_org,		do_org_list	},
  { "PAGE",	0, 0, do_page,		NULL		},
  { "RADIX",	0, 0, do_radix,		NULL		},
  { "RADX",	0, 0, do_radix,		NULL		},
  { "REPEAT",	0, 0, do_repeat,	NULL		},
  { "SBTTL",	0, 0, do_subttl,	NULL		},
  { "STITLE",	0, 0, do_subttl,	NULL		},
  { "STR",	0, 1, do_byte,		NULL		},
  { "STRING",	0, 1, do_byte,		NULL		},
  { "SUBTTL",	0, 0, do_subttl,	NULL		},
  { "SYM",	0, 0, do_syms,		NULL		},
  { "SYMS",	0, 0, do_syms,		NULL		},
  { "TITLE",	0, 0, do_title,		NULL		},
  { "WARN",	0, 1, do_warn,		NULL		},
  { "WARNING",	0, 1, do_warn,		NULL		},
  { "WIDTH",	0, 0, do_width,		NULL		},
  { "WORD",	0, 0, do_word,		NULL		},
  { NULL				     		}
};


const pseudo_t *
is_pseudo(const char *name, int dot)
{
    char id[ID_LEN], *p;
    const pseudo_t *ptr;
    int i;

    /* Convert the mnemonic to uppercase. */
    p = id;
    while (*name != '\0')
        *p++ = (char)toupper(*name++);
    *p = '\0';

    for (ptr = pseudos; ptr->name != NULL; ptr++) {
	if ((i = strcmp(ptr->name, id)) == 0) {
		if (ptr->dotted && !dot)
			break;

		return ptr;
	}

	if (i > 0)
		break;
    }

    return NULL;
}


char *
pseudo(const pseudo_t *op, char **p, int pass)
{
    char *newp = NULL;

    if (op == NULL)
	error(ERR_NODIRECTIVE, NULL);

    if (op->always || ifstate)
	newp = op->func(p, pass);

    /* Just skip. */
    while (! IS_END(**p))
	(*p)++;

    return newp;
}


char *
pseudo_list(const struct pseudo *op, char *str)
{
    char *ret = NULL;

    if (op != NULL && op->list != NULL && (op->always || ifstate))
	ret = op->list(str);

    return ret;
}
