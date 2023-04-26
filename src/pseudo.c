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
 * Version:	@(#)pseudo.c	1.0.3	2023/04/25
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


typedef struct pseudo {
    const char	*name;
    int		always;
    char	*(*func)(char **, int);
    void	*lister;
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
		emit_str(buf, (uint16_t)len, pass);
		pc += (uint16_t)len;
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
		emit_str(buf, (uint16_t)len, pass);
		pc += (uint16_t)len;
	} else if (**p == '\'') {
		(*p)++;
		if (**p == '\'')
			error(ERR_CHR, NULL);

		buf[0] = **p;

		(*p)++;
		if (**p != '\'')
			error(ERR_CHREND, NULL);
		len = 1;
		emit_str(buf, (uint16_t)len, pass);

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
	SET_TYPE(v, TYPE_BYTE);
    }

    define_variable(id, v);

    return NULL;
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

    if (pass == 1) {
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

    if (iflevel > 0) {
	newifstate = !ifstate;
	ifstate = ifstack[iflevel - 1];
    } else
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
    }

    return NULL;
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


/* The ".error <text>" directive. */
static char *
do_error(char **p, int pass)
{
    char buff[STR_LEN * 4];

    skip_white(p);
    string_lit(p, buff, STR_LEN * 4, 0);

    error(ERR_USER, buff);
    /*NOTREACHED*/

    return NULL;
}


/* The ".equ <expr>" directive. */
static char *
do_equ(char **p, int pass)
{
    char id[ID_LEN];
    value_t v;

    ident(p, id);

    v = expr(p);

    define_variable(id, v);

    return NULL;
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
	//FIXME: not needed for ihex/srec output!
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

    /*
     * Optionally, "fill out" the space with $00 bytes.
     *
     * Note that we do NOT do this the first time the origin
     * is changed (from being $0000) since that would not be
     * sensible. We only do it if we change it on the fly.
     */
    if (opt_F) {
	if (org_done) {
		while (pc < v.v) {
			//FIXME: not needed for ihex/srec output!
			emit_byte(0x00, pass);

			pc++;
		}
	} else {
		/* Remember our load address. */
		emit_addr(v.v);

		org_done = 1;
	}
    }

    /* Now set the new origin. */
    pc = v.v;

    return NULL;
}


/* The ".page [<width][,<height>]" directive. */
static char *
do_page(char **p, int pass)
{
    value_t v;

    skip_white_and_comment(p);
    if (IS_END(**p)) {
	/* This was just a .page to start a new page. */
	list_page(NULL);
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
	emit_word(v.v, pass);

	pc += 2;
	skip_white(p);
	if (**p == ',') {
		skip_curr_and_white(p);
		next = 1;
	}
    } while (next);

    return NULL;
}


/* The ".warn <text>" directive. */
static char *
do_warn(char **p, int pass)
{
    char buff[STR_LEN * 4];
    char *str = *p;

    skip_white(p);
    string_lit(p, buff, STR_LEN * 4, 0);

    if (pass == 2)
	printf("*** WARNING: ");

    *p = str;
    do_echo(p, pass);

    return NULL;
}


static const pseudo_t pseudos[] = {
  { "ALIGN",	0, do_align,	NULL	},
  { "ASCIIZ",	0, do_asciz,	NULL	},
  { "ASCIZ",	0, do_asciz,	NULL	},
  { "BINARY",	0, do_blob,	NULL	},
  { "BLOB",	0, do_blob,	NULL	},
  { "BYTE",	0, do_byte,	NULL	},
  { "CPU",	0, do_cpu,	NULL	},
  { "DATA",	0, do_byte,	NULL	},
  { "DB",	0, do_byte,	NULL	},
  { "DEFINE",	0, do_define,	NULL	},
  { "DWORD",	0, do_dword,	NULL	},
  { "DL",	0, do_dword,	NULL	},
  { "DW",	0, do_word,	NULL	},
  { "ECHO",	0, do_echo,	NULL	},
  { "ELSE",	1, do_else,	NULL	},
  { "END",	0, do_end,	NULL	},
  { "ENDIF",	1, do_endif,	NULL	},
  { "ERROR",	0, do_error,	NULL	},
  { "EQU",	0, do_equ,	NULL	},
  { "FI",	1, do_endif,	NULL	},
  { "FILL",	0, do_fill,	NULL	},
  { "IF",	1, do_if,	NULL	},
  { "IFDEF",	1, do_ifdef,	NULL	},
  { "IFN",	1, do_ifn,	NULL	},
  { "IFNDEF",	1, do_ifndef,	NULL	},
  { "INCLUDE",	0, do_include,	NULL	},
  { "ORG",	0, do_org,	NULL	},
  { "PAGE",	0, do_page,	NULL	},
  { "RADIX",	0, do_radix,	NULL	},
  { "RADX",	0, do_radix,	NULL	},
  { "STRING",	0, do_byte,	NULL	},
  { "STR",	0, do_byte,	NULL	},
  { "TITLE",	0, do_title,	NULL	},
  { "SYMS",	0, do_syms,	NULL	},
  { "SYM",	0, do_syms,	NULL	},
  { "WARN",	0, do_warn,	NULL	},
  { "WORD",	0, do_word,	NULL	},
  { NULL				}
};


const pseudo_t *
is_pseudo(const char *name)
{
    const pseudo_t *ptr;
    int i;

    for (ptr = pseudos; ptr->name != NULL; ptr++) {
	if ((i = strcmp(ptr->name, name)) == 0)
		return ptr;
	if (i > 0)
		break;
    }

    return NULL;
}


char *
pseudo(const pseudo_t *ptr, char **p, int pass)
{
    char *newp = NULL;

    if (ptr == NULL)
	error(ERR_NODIRECTIVE, NULL);

    if (ptr->always || ifstate)
	newp = ptr->func(p, pass);

    /* Just skip. */
    while (! IS_END(**p))
	(*p)++;

    return newp;
}
