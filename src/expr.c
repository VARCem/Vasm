/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle all expressions.
 *
 * Version:	@(#)expr.c	1.0.4	2023/04/23
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "global.h"


#ifndef isxdigit
# define isxdigit(x)	(isdigit((x)) || (((x) >= 'a') && ((x) <= 'f')) || \
					(((x) >= 'A') && ((x) <= 'F')))
#endif


static int
starts_with(char *text, char *s)
{
    while (! IS_END(*s)) {
	if (toupper(*text++) != toupper(*s++))
		return 0;
    }

    return 1;
}


static int
digit(const char *p)
{
    if (*p <= '9')
	return (int)(*p - '0');
    if (*p <= 'F')
	return (int)(*p + 10 - 'A');

    return (int)(*p + 10 - 'a');
}


/* Read a number in some radix. */
static value_t
number(char **p)
{
    value_t num = { 0 };
    uint8_t type = 0;
    char *pt = *p;

    if (**p == '&') {
	(*p)++;
do_dec:
	if (! isdigit(**p))
		error(ERR_NUM, NULL);
	do {
		num.v = num.v * 10 + digit((*p)++);
	} while (isdigit(**p));

	type = NUM_TYPE(num.v);
    } else if (**p == '\\') {
	(*p)++;
do_oct:
	if (! (**p >= 0 && **p <= '7'))
		error(ERR_NUM, NULL);
	do {
		num.v = num.v * 8 + digit((*p)++);
	} while (**p >= '0' && **p <= '7');

	type = NUM_TYPE(num.v);
    } else if (**p == '$') {
	(*p)++;
do_hex:
	if (! isxdigit(**p))
		error(ERR_NUM, NULL);
	do {
		num.v = (num.v << 4) + digit((*p)++);
	} while (isxdigit(**p));

	type = ((*p - pt) > 7) ? TYPE_DWORD : ((*p - pt) > 3) ? TYPE_WORD : NUM_TYPE(num.v);
    } else if (**p == '%') {
	(*p)++;
do_bin:
	if ((**p != '0') && (**p != '1'))
		error(ERR_NUM, NULL);
	do {
		num.v = (num.v << 1) + digit((*p)++);
	} while ((**p == '0') || (**p == '1'));

	type = ((*p - pt) > 17) ? TYPE_DWORD : ((*p - pt) > 9) ? TYPE_WORD : NUM_TYPE(num.v);
    } else {
	/* No explicit radix specifier present, go with the default. */
	switch (radix) {
		case 2:
			goto do_bin;

		case 8:
			goto do_oct;

		case 10:
			goto do_dec;

		case 16:
			goto do_hex;
	}

	/*NOTREACHED*/
    }

    SET_TYPE(num, type);
    SET_DEFINED(num);

    return num;
}


static value_t
primary(char **p, int label)
{
    char id[ID_LEN], id2[ID_LEN], *pt;
    value_t res = { 0 };
    symbol_t *sym;

    skip_white(p);

#ifdef _DEBUG
    if (opt_d)
	printf("> PRIMARY(%s)\n", dumpline(*p));
#endif
    pt = *p;
    if (**p == '(') {
	/* Should we keep a nesting count? */
	(*p)++;
	res = expr(p);

	skip_white(p);

	if (**p != ')')
		error(ERR_UNBALANCED, NULL);
	(*p)++;
    } else if (**p == DOT_CHAR) {
	/* Check for functions. */
	(*p)++;
	pt = *p;
	if (isalpha(**p)) {
		ident_upcase(p, id);
		if (**p == '(') {
			/* We have an alnum AND a (, must be function. */
			(*p)++;

			/* Execute function. */
			res = function(id, p);

			if (**p != ')')
				error(ERR_OPER, NULL);
			(*p)++;
		} else {
			/* We have alnum but no (, must be a dot label */
			*p = pt;
			goto dot_label;
		}
	} else {
dot_label:
		if (isalnum(**p) || IS_IDENT(**p)) {
			/* Dot labels require preceeding global label. */
			if (current_label == NULL)
				error(ERR_NO_GLOBAL, NULL);

			/* OK, create full global label. */
			(*p)--;
			nident(p, id2);
			if ((strlen(current_label->name) + strlen(id2)) >= ID_LEN)
				error(ERR_IDLEN, NULL);
			strcpy(id, current_label->name);
			strcat(id, id2);
			sym = sym_lookup(id, NULL);
			if (sym != NULL) {
				res = sym->value;
			} else {
				res.v = 0;
				res.t = 0;
			}
		} else {
			/* Current program counter. */
			res.v = pc;
			res.t = TYPE_WORD | VALUE_DEFINED;
		}
	}
    } else if (**p == '@') {
	/* Current program counter or local label. */
	(*p)++;
	if (isalnum(**p)) {
		/* Most likely a local label. */
		if (current_label == NULL)
			error(ERR_NO_GLOBAL, NULL);
		nident(p, id);
		sym = sym_lookup(id, &current_label->locals);
		if (sym != NULL) {
			res = sym->value;
		} else {
			res.v = 0;
			res.t = 0;
		}
	} else {
		/* Current program counter. */
program_counter:
		res.v = pc;
		res.t = TYPE_WORD | VALUE_DEFINED;
	}
    } else if (**p == '*') {
	/* Current program counter. */
	(*p)++;
	goto program_counter;
    } else if ((**p == '$') && (! isxdigit(*(pt+1)))) {
	(*p)++;
	goto program_counter;
    } else if (**p == '\'') {
	/* Single character. */
	(*p)++;
	if (is_end(**p) || (**p < 0x20))
		error(ERR_CHR, NULL);

	res.v = **p;
	res.t = TYPE_BYTE | VALUE_DEFINED;

	(*p)++;
	if (**p != '\'')
		error(ERR_CHR, NULL);
	(*p)++;
    } else if (isalpha(**p)) {
	/* Symbol reference. */
	nident(p, id);
	sym = sym_lookup(id, NULL);
	if (sym == NULL) {
		/* Forward reference, remember it. */
		sym = sym_aquire(id, NULL);
		if (DEFINED(sym->value))
			error(ERR_REDEF, id);

		/* Set the correct type. */
		if (label) {
			sym->kind = KIND_LBL;
			sym->value.t = TYPE_WORD;
		} else {
			sym->kind = KIND_VAR;
			sym->value.t = TYPE_BYTE;
		}
		sym->value.v = 0;
	}
	res = sym->value;
    } else {
	/* Must be just a number, but do mind the radix! */
	res = number(p);
    }

    return res;
}


/*
 * For compatibility with other assemblers,
 * we add word-based operators for:
 *
 *  %    MOD
 *  &    AND
 *  <<   ASL
 *  >>   ASR
 *
 * for completeness.
 */
static value_t
product(char **p)
{
    value_t n2, res;
    char op, op2;

#ifdef _DEBUG
    if (opt_d)
	printf("PRODUCT(%s)\n", dumpline(*p));
#endif
    res = primary(p, 1);

    skip_white(p);

    op = **p; op2 = *((*p)+1);
    if (starts_with(*p, "MOD ")) { op = '%'; *p += 4; }
    else if (starts_with(*p, "AND ")) { op = '&'; *p += 4; }
    else if (starts_with(*p, "ASL ")) { op = '<'; op2 = '<'; *p += 4; }
    else if (starts_with(*p, "ASR ")) { op = '>'; op2 = '>'; *p += 4; };

    while ((op == '*') || (op == '&') || (op == '/') || (op == '%') ||
	   (op == '<' && op2 == '<') || (op == '>' && op2 == '>') ||
	   (op == '|' && op2 == '|') || (op == '&' && op2 == '&')) {

	/* Skip operator. */
	(*p)++;

	/* For double-character operators, skip the second character. */
	if ((op == '<' || op == '>' || op == '|') ||
	    (op == '&' && op2 == '&'))
		(*p)++;

	n2 = primary(p, 1);

	switch (op) {
		case '*':	// multiply
			res.v = (uint16_t)(res.v * n2.v);
			break;

		case '/':	// divide
			if (n2.v == 0)
				error(ERR_ZERO, NULL);
			res.v = (uint16_t)(res.v / n2.v);
			break;

		case '%':	// modulo
			if (n2.v == 0)
				error(ERR_ZERO, NULL);
			res.v = (uint16_t)(res.v % n2.v);
			break;

		case '&':	// AND
			if (op2 == op)		// logical AND
				res.v = !!res.v && !!n2.v;
			else			// bitwise AND
				res.v = (uint16_t)(res.v & n2.v);
			break;

		case '|':	// OR
			if (op2 == op)		// logical OR
				res.v = !!res.v || !!n2.v;
			else			// bitwise OR
				res.v = (uint16_t)(res.v | n2.v);
			break;

		case '<':	// bitwise shift LEFT
			res.v = res.v << n2.v;
			break;

		case '>':	// bitwise shift RIGHT
			res.v = res.v >> n2.v;
			break;
	}
	INFER_TYPE(res, n2);
	INFER_DEFINED(res, n2);

	skip_white(p);

	op = **p;
	op2 = *((*p)+1);
    }

    return res;
}


/*
 * For compatibility with other assemblers,
 * we add word-based operators for:
 *
 *  |    OR
 *  ^    XOR (and/or EOR)
 *
 * for completeness.
 */
static value_t
term(char **p)
{
    value_t n2, res;
    char op;

    skip_white(p);

#ifdef _DEBUG
    if (opt_d)
	printf("TERM(%s)\n", dumpline(*p));
#endif
    if (**p == '-') {		// indicate negative value
	/* Unary minus. */
	(*p)++;
	res = product(p);
	res.v = -res.v;
    } else {
	/* Unary plus. */
	if (**p == '+')		// skip, default is positive
		(*p)++;

	res = product(p);
    }

    skip_white(p);

    op = **p;
    if (starts_with(*p, "OR ")) { op = '|'; *p += 3; }
    else if (starts_with(*p, "XOR ")) { op = '^'; *p += 4; }
    else if (starts_with(*p, "EOR ")) { op = '^'; *p += 4; };

    while ((op == '+') || (op == '-') || (op == '|') || (op == '^')) {
	/* Skip operator. */
	(*p)++;

	n2 = product(p);

	switch (op) {
		case '+':	// add
			res.v = res.v + n2.v;
			break;

		case '-':	// subtract
			res.v = res.v - n2.v;
			break;

		case '|':	// bitwise OR
			res.v = res.v | n2.v;
			break;

		case '^':	// bitwise XOR
			res.v = res.v ^ n2.v;
			break;
	}
	INFER_TYPE(res, n2);
	INFER_DEFINED(res, n2);

	skip_white(p);

	op = **p;
    }

    return res;
}


static value_t
compare(char **p)
{
    value_t res, n2;
    char op, op2;

#ifdef _DEBUG
    if (opt_d)
	printf("COMPARE(%s)\n", dumpline(*p));
#endif
    res = term(p); 

    skip_white(p);

    op = **p; op2 = *(*p + 1);
    while ((op == '<') || (op == '>') ||
	   (op == '=' && op2 == '=') || (op == '!' && op2 == '=') ||
	   (op == '<' && op2 == '=') || (op == '>' && op2 == '=')) {

	/* Skip operator. */
	(*p)++;

	/* For double-character operators, skip the second character. */
	if ((op == '=') || (op == '!') ||
	    (op == '<' && op2 == '=') || (op == '>' && op2 == '='))
		(*p)++;

	n2 = term(p);
	switch (op) {
		case '=':
			res.v = res.v == n2.v;
			break;

		case '!':
			res.v = res.v != n2.v;
			break;

		case '<':
			res.v = (op2 == '=') ? res.v <= n2.v : res.v < n2.v;
			break;

		case '>':
			res.v = (op2 == '=') ? res.v >= n2.v : res.v > n2.v;
			break;
	}

	INFER_DEFINED(res, n2);
	if (DEFINED(res) && res.v)
		res.v = 1;
	SET_TYPE(res, TYPE_BYTE);

	skip_white(p);

	op = **p;
	op2 = *((*p)+1);
    }

    return res;
}


/*
 * For compatibility with other assemblers,
 * we add word-based operators for:
 *
 *  !    NOT
 *  ~    NOT
 *
 * for completeness.
 */
value_t
expr(char **p)
{
    value_t v;
    char op;

    skip_white(p);

#ifdef _DEBUG
    if (opt_d)
	printf("EXPR(%s)\n", dumpline(*p));
#endif

    op = **p;
    if (starts_with(*p, "NOT ")) { op = '~'; *p += 4; };

    if (op == '>') {
	/* High-byte (MSB) operator. */
	(*p)++;
	v = compare(p);
	v.v = v.v >> 8;
	SET_TYPE(v, TYPE_BYTE);
    } else if (op == '<') {
	/* Low-byte (LSB) operator. */
	(*p)++;
	v = compare(p);
	v.v = v.v & 0xff;
	SET_TYPE(v, TYPE_BYTE);
    } else if ((op == '!') || (op == '~')) {
	/* NOT operators. */
	(*p)++;
	v = term(p);
	v.v = ~v.v;
	SET_TYPE(v, TYPE_WORD);
    } else if (starts_with(*p, "[b]")) {
	/* Lossless conversion to byte. */
	*p += 3;
	v = compare(p);
	if (DEFINED(v) && v.v > 0xff)
		error(ERR_RNG_BYTE, NULL);
	SET_TYPE(v, TYPE_BYTE);
    } else if (starts_with(*p, "[d]")) {
	/* Lossless conversion to doubleword. */
	*p += 3;
	v = compare(p);
	SET_TYPE(v, TYPE_DWORD);
    } else if (starts_with(*p, "[w]")) {
	/* Lossless conversion to word. */
	*p += 3;
	v = compare(p);
	SET_TYPE(v, TYPE_WORD);
    } else {
	/* Iterate. */
	v = compare(p);
    }

    return v;
}


/* Take a value and try to convert it to a byte value. */
value_t
to_byte(value_t v)
{
    if (DEFINED(v) && (v.v > 0xff))
	error(ERR_RNG_BYTE, NULL);

    SET_TYPE(v, TYPE_BYTE);

    return v;
}


/* Take a value and try to convert it to a word value. */
value_t
to_word(value_t v)
{
    if (DEFINED(v) && (v.v > 0xffff))
	error(ERR_RNG_WORD, NULL);

    SET_TYPE(v, TYPE_WORD);

    return v;
}


char
value_type(uint8_t type)
{
    switch (type & TYPE_MASK) {
	case TYPE_BYTE:
		return 'B';

	case TYPE_WORD:
		return 'W';

	case TYPE_DWORD:
		return 'D';
    }

    return '?';
}


/* Determine the desired format for printing a value. */
int
value_format(char **p)
{
    char *pt = *p;
    int fmt = 0;

    skip_white(p);
    if (**p != FMT_B_CHAR) {
	*p = pt;
	return 0;
    }

    skip_curr_and_white(p);

    switch (**p) {
	case FMT_BIN_CHAR:
	case FMT_DEC_CHAR:
	case FMT_HEX_CHAR:
	case FMT_HEX1_CHAR:
	case FMT_HEX2_CHAR:
		fmt = (int)**p;
		(*p)++;
		break;

	case FMT_E_CHAR:
		fmt = FMT_DEC_CHAR;
		break;

	default:
		*p = pt;
		return -ERR_FMT;
    }

    if (**p != FMT_E_CHAR) {
	*p = pt;
	return -ERR_FMT;
    }

    (*p)++;

    return fmt;
}


char *
value_print(value_t v)
{
    static char buff[9];

    switch (TYPE(v)) {
	case TYPE_BYTE:
		sprintf(buff, "%02X", v.v & 0xff);
		break;

	case TYPE_WORD:
		sprintf(buff, "%04X", v.v & 0xffff);
		break;

	case TYPE_DWORD:
		sprintf(buff, "%08X", v.v);
		break;
    }

    return buff;
}


/* Print a value in a specific format. */
char *
value_print_format(value_t v, int fmt)
{
    static char buff[33];
    char *bufp = buff;
    int len = (v.t & TYPE_DWORD) ? 32 : (v.t & TYPE_WORD) ? 16 : 8;
    int i;

    switch (fmt) {
	case FMT_BIN_CHAR:
		for (i = 0; i < len; i++)
			*bufp++ = (v.v & (1 << ((len - i) - 1))) ? '1':'0';
		*bufp = '\0';
		break;

	case FMT_DEC_CHAR:
		sprintf(bufp, "%u", (unsigned)v.v);
		break;

	case FMT_HEX_CHAR:
		sprintf(bufp, "$%0*X", len / 4, (unsigned)v.v);
		break;

	case FMT_HEX1_CHAR:
		sprintf(bufp, "0x%0*x", len / 4, (unsigned)v.v);
		break;

	case FMT_HEX2_CHAR:
		sprintf(bufp, "0x%0*X", len / 4, (unsigned)v.v);
		break;
    }

    return buff;
}
