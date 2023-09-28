/*
 * VASM		VARCem Multi-Target Macro Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle macros.
 *
 * Version:	@(#)macro.c	1.0.1	2023/08/20
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "error.h"


#define MACRO_SIZE	1024			// max #bytes in macro def
#define PARAM_SIZE	128			// max #chars in parameters


typedef struct macro {
    char	name[ID_LEN+1];

    char	formal[PARAM_SIZE],		// formal (def) parameters
		actual[PARAM_SIZE];		// actual (call) parameters

    char	*defptr;			// next character in definition
    char	*dataptr;			// next character in data
    char	*saved;				// saved source pointer

    char	def[MACRO_SIZE],		// macro definition text
		data[MACRO_SIZE];		// macro text

    struct macro *next;
} macro_t;


int		macstate,
		newmacstate,
		maclevel;

static macro_t	*macros = NULL,
		*curmac = NULL;


/* Update the matched string with new contents. */
static void
change(char *text_ptr, const char *srchs, const char *repls)
{
    char lastpart[1024];
    int srchlen;

    srchlen = strlen(srchs);
    strcpy(lastpart, text_ptr + srchlen);
    strcpy(text_ptr, repls);
    strcat(text_ptr, lastpart);
}


/* Find a string match. */
static char *
match(const char *start_ptr, const char *searchstr)
{
    int i;

    while (*start_ptr) {
	i = 0;
	while (*(start_ptr+i) == *(searchstr+i)) {
		if (*(searchstr+i) == '\0')
			break;
		i++;
	}
	if (*(searchstr+i) == '\0')
		break;
	start_ptr++;
    }

    if (*start_ptr == '\0')
	start_ptr = NULL;

    return (char *)start_ptr;
}


/* Substitute all parameters in the current text line. */
static void
subst(macro_t *m, char *ptr)
{
    char buff[MACRO_SIZE];
    char srchs[PARAM_SIZE];
    char repls[PARAM_SIZE];
    char *f, *a, *x;
    char *src_ptr;

    f = m->formal;
    a = m->actual;
    if (*f && *a) {
	strcpy(buff, ptr);

	while (*f && *a) {
		/* Copy one formal parameter. */
		x = srchs;
		while (*f && *f != ',')
			*x++ = *f++;
		*x = '\0';
		if (*f)
			f++;

		/* Copy one actual parameter. */
		x = repls;
		while (*a && *a != ',')
			*x++ = *a++;
		*x = '\0';
		if (*a)
			a++;

		/* Perform the substitutions for this parameter. */
		src_ptr = buff;
		while ((src_ptr = match(src_ptr, srchs)) != NULL)
			change(src_ptr, srchs, repls);
	}

	if (*f && !*a)
		error(ERR_MACACT, NULL);
	if (!*f && *a)
		error(ERR_MACFRM, NULL);

	/* Copy text line back to caller. */
	strcpy(ptr, buff);
    }
}


/* Reset macros for each pass. */
void
macro_reset(void)
{
    macro_t *m, *ptr;

    for (ptr = macros; ptr != NULL; ) {
	m = ptr->next;
	free(ptr);
	ptr = m;
    }

    macros = NULL;
}


/* Add a line to the current macro definition. */
void
macro_add(const char *p)
{
#if 1
//FIXME: this is horrible.
    const char *xx = p;

    while (*xx && (*xx == ' ' || *xx == '\t'))
	xx++;
    if (*xx == '.')
	xx++;
# ifdef _MSC_VER
    if (!strncmp(xx, "endm", 4) || !strncmp(xx, "ENDM", 4))
# else
    if (!strncasecmp(xx, "endm", 4))
# endif
	return;
#endif

    /* Add this line (including spacings) to the macro definition. */
    while (! IS_END(*p))
	*curmac->defptr++ = *p++;
    *curmac->defptr++ = '\n';
}


/* Check if this is a valid macro name. */
int
macro_ok(const char *name)
{
    macro_t *m;

    for (m = macros; m != NULL; m = m->next)
	if (! strcasecmp(m->name, name))
		return 1;

    return 0;
}


/* Execute a macro. */
void
macro_exec(const char *name, char **p, char **newp, int pass)
{
    char temp[1024], *sp;
    macro_t *m;

    for (m = macros; m != NULL; m = m->next)
	if (! strcasecmp(m->name, name))
		break;
    if (m == NULL)
	return;
    curmac = m;

    /* Save actual parameters, but do not keep comments. */
    sp = curmac->actual;
    while (! IS_END(**p)) {
	if (**p != COMMENT_CHAR) {
		*sp++ = **p;
		(*p)++;
	} else {
		/* See if we can remove trailing whitespace. */
		while (IS_SPACE(*(sp-1)))
			*(--sp) = '\0';

		skip_white_and_comment(p);
	}
    }
    *sp = '\0';

    /* Save the current line pointer so we know where to return to. */
    curmac->saved = *p;

    /*
     * We now copy the macro's definition (which is in m->def)
     * to a new buffer (in m->data), while trying to expand the
     * parameters for each line.
     */
    curmac->defptr = curmac->def;
    curmac->dataptr = curmac->data;
    while (*curmac->defptr) {
	/* Copy one line. */
	sp = temp;
	do {
		*sp++ = *curmac->defptr;
	} while (*curmac->defptr && *curmac->defptr++ != '\n');
	*sp = '\0';

	/* Expand the copy. */
	subst(curmac, temp);

	/* Copy expanded line to data. */
	sp = temp;
	while (*sp)
		*curmac->dataptr++ = *sp++;
    }

    /* Set up a new "line pointer" for the parser. */
    curmac->dataptr = curmac->data;
    *newp = curmac->dataptr;
}


/* Reached end of macro, switch back to source. */
void
macro_close(char **p)
{
    if (curmac != NULL) {
	*p = curmac->saved;
printf("CONT(%s)\n", *p);

	curmac = NULL;
	maclevel--;
    }
}


/* The ".macro name[,arg,...]" directive. */
char *
do_macro(char **p, int pass)
{
    char temp[1024];
    char *sp = temp;
    macro_t *m, *ptr;

    skip_white(p);
    if (IS_END(**p))
	error(ERR_EOL, NULL);

    if (macstate)
	error(ERR_MACRO, NULL);

    /* We do need a macro name. */
    if (current_label == NULL)
	error(ERR_LABEL, NULL);

    /* Make sure label does not have a colon. */
    if (current_label->subkind == 2)
	error(ERR_MACNAME, NULL);

    current_label->kind = KIND_MAC;

    /* Get to the macro parameters. */
    skip_white(p);
    while (! IS_END(**p)) {
	if (sp >= &temp[sizeof(temp) - 2])
		error(ERR_MEM, "macro parameters");

	*sp++ = **p;
	(*p)++;
    }
    *sp = '\0';

    /* Allocate a new macro. */
    m = malloc(sizeof(macro_t));
    if (m == NULL)
	error(ERR_MEM, "new macro");
    memset(m, 0x00, sizeof(macro_t));
    strcpy(m->name, current_label->name);
    strcpy(m->formal, temp);
    m->defptr = m->def;

    /* Insert in alphabetical order. */
    if ((macros == NULL) || (strcasecmp((macros)->name, m->name) > 0)) {
	m->next = macros;
	macros = m;
    } else {
	for (ptr = macros; ptr->next != NULL; ptr = ptr->next)
		if (strcasecmp(ptr->next->name, m->name) > 0)
			break;

	m->next = ptr->next;
	ptr->next = m;
    }

    /* We are now defining a new macro. */
    curmac = m;
    newmacstate = 1;

    return NULL;
}


/* The ".endm" directive. */
char *
do_endm(char **p, int pass)
{
    skip_white(p);
    if (! IS_END(**p))
	error(ERR_EOL, NULL);

    if (! macstate)
	error(ERR_ENDM, NULL);

    /* Terminate the macro. */
    *curmac->defptr++ = ETX_CHAR;
    *curmac->defptr = '\0';

    /* No longer defining a new macro. */
    newmacstate = 0;
    curmac = NULL;

    return NULL;
}
