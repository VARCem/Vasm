/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the listfile output.
 *
 * Version:	@(#)list.c	1.0.10	2023/06/17
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
#include <time.h>
#include "global.h"


#define LIST_PLENGTH	66		// number of lines per page
#define LIST_PWIDTH	80		// number of characters per line

#define LIST_CHAR_FF	"\014"		// FormFeed character
#define LIST_CHAR_SI	"\017"		// SI (condensed printing mode)
#define LIST_CHAR_DC2	"\022"		// DC2 (end condensed printing mode)

#define LIST_NBYTES	4		// this many code bytes per line


int		list_plength = LIST_PLENGTH,	// lines per page
		list_pwidth = LIST_PWIDTH;	// characters per line

static int	list_lnr,
		list_pnr,
		list_pln;
static uint32_t	list_pc,
		list_oc;
static char	*list_title;
static char	*list_subttl;
static int	list_syms = 0;
static FILE	*list_file = NULL;
static char	list_path[1024];


void
list_set_head(const char *str)
{
    if (list_title != NULL)
        free(list_title);
    list_title = NULL;

    /* .. and set the new one. */
    if (str != NULL)
	list_title = strdup(str);
}


void
list_set_head_sub(const char *str)
{
    if (list_subttl != NULL)
        free(list_subttl);
    list_subttl = NULL;

    /* .. and set the new one. */
    if (str != NULL)
	list_subttl = strdup(str);
}


void
list_set_syms(int syms)
{
    list_syms = syms;
}


/*
 * Start a new page in the listing file.
 *
 * We have the dimensions set in PLENGTH (66) and PWIDTH (80),
 * which can be changed with the .page directive.  Do remember
 * that these numbers INCLUDE the page margings, so effectively
 * we have less than those- most printers require about 0.25"
 * of margings at the minimum, so a U.S. Letter page of 8.5x11"
 * will result in 80 (at 10cpi) characters and 63 (at 6lpi) lines
 * for the page. We also use 3 lines per page for the header, so
 * with those default settings, we get to "keep" 60 lines.
 */
void
list_page(const char *head, const char *sub)
{
    char buff[1024], page[256];
    char temp[1024], date[64];
    char *ptr = buff;
    struct tm *tm;
    time_t now;
    int i, skip;

    if (list_file == NULL)
	return;

    if (head == NULL)
	head = list_title;

    /* Initialize printer to condensed mode if width > 80. */
    if (opt_P && list_pnr == 0 && list_pwidth > 80) {
	fprintf(list_file, LIST_CHAR_SI);
    }

    /* Bump the page number. */
    list_pnr++;

    /* Get a (localized) date and time string. */
    (void)time(&now);
    tm = localtime(&now);
    strftime(date, sizeof(date), "%c", tm);

    sprintf(page, "%s    Page %i", date, list_pnr);
    skip = list_pwidth - (strlen(myname) + 1 + strlen(version) + strlen(page));
    if (list_pnr > 1) {
	/* Insert a form-feed for all but first page. */
	sprintf(ptr, "%s", LIST_CHAR_FF);
	ptr += strlen(ptr);
    }
    sprintf(ptr, "%s %s", myname, version);
    ptr += strlen(ptr);
    while (skip--)
	*ptr++ = ' ';
    strcpy(ptr, page);
    fprintf(list_file, "%s\n", buff);

    sprintf(page, "File: %s", filenames[filenames_idx]);
    i = strlen(page);
    memset(temp, 0x00, sizeof(temp));
    if (head != NULL)
	strncpy(temp, head, sizeof(temp) - 1);
    if (sub != NULL) {
	strcat(temp, " : ");
	strcat(temp, sub);
    }
    skip = list_pwidth - i;
    if (skip < strlen(temp)) {
	/* Title is too long, we have to clip it. */
	temp[skip - 1] = '\0';
    }
    skip = list_pwidth - (i + strlen(temp));
    ptr = buff;
    sprintf(ptr, "%s", temp);
    ptr += strlen(ptr);
    while (skip--)
	*ptr++ = ' ';
    strcpy(ptr, page);
    fprintf(list_file, "%s\n\n", buff);

    /* OK, good for another page.. */
    list_pln = list_plength - (3 + 3);	// margin + header
}


/*
 * Generate one line of listing info.
 *
 * Keep in mind that many source files contain TAB characters,
 * and for those to look "nice" in our listings, we must make
 * sure the actual listing starts at a multiple-of-8 position
 * plus one. For now, we are using position 33.
 */
void
list_line(const char *p)
{
    char temp[128];
    int count = 0;
    char *sp;

    if (list_file == NULL)
	return;

    if (list_pln == 0) {
	/* Reset the page. */
	list_page(list_title, list_subttl);
    }

    /* Output listing line number. */
    fprintf(list_file, "%05i ", list_lnr++);
    fprintf(list_file, "%06X", list_pc);

    /* Our max space is LIST_NBYTES * 3 characters. */
    count = LIST_NBYTES * 3;

    /* Output code if we emitted any. */
    if (list_oc < output_size) {
	/* List the generated bytes. */
	while (list_oc < output_size && count > 0) {
		fprintf(list_file, " %02X", output_buff[list_oc++]);
		list_pc++;
		count -= 3;
	}
    } else {
	/* No code generated, check if a directive wants something listed. */
	sp = pseudo_list(psop, temp);
	if (sp != NULL) {
		fputc(' ', list_file);
		count--;

		if (strlen(sp) > count)
			sp[count] = '\0';

		while (*sp != '\0') {
			fputc(*sp++, list_file);
			count--;
		}
	}
    }

    /* Fill up the remaining space. */
    while (count--)
	fputc(' ', list_file);

    fprintf(list_file, "%6i%c ", line, ifstate ? ':' : '-');

    if (p != NULL) {
	while (*p && *p != '\n' && *p != EOF_CHAR)
		fputc(*p++, list_file);
    }

    /* End the line, and start a new one. */
    fputs("\n", list_file);
    if (list_plength != 255)
	list_pln--;

    /* If not all generated bytes were emitted, do this again. */
    if (list_oc < output_size)
	list_line(NULL);
}


void
list_symbols(void)
{
    symbol_t *loc, *sym;
    FILE *fp;

    /* Has this been enabled? */
    if (! list_syms)
	return;

    /* If we are not using a listing file, dump to stdout. */
    if (list_file != NULL) {
	list_page("** SYMBOL TABLE **", NULL);	// new page
	fp = list_file;				// use listing file
    } else
	fp = stdout;				// use stdout

    sym = sym_table();
    if (sym == NULL) {
	fprintf(fp, "No symbols defined.\n");
	return;
    }

    if (fp == stdout)				// only when on stdout
	fprintf(fp, "Symbol table:\n");

    for (; sym; sym = sym->next) {
	/* Skip symbols with __ prefix. */
	if (!opt_v && !strncmp(sym->name, "__", 2))
		continue;

	if ((fp != stdout) && (list_pln == 0))
		list_page("** SYMBOL TABLE **", NULL);

	fprintf(fp, "%-32s %c ", sym->name, sym_type(sym));
	if (DEFINED(sym->value)) {
		fprintf(fp, "%9s ", value_print(sym->value));
		if (IS_VAR(sym))
			fprintf(fp, "%c", value_type(sym->value));
		else
			fprintf(fp, " ");
		fprintf(fp, "        ");
		if (sym->linenr < 0)
			fprintf(fp, "-builtin-");
		else if (sym->filenr != -1 && sym->linenr != 0)
			fprintf(fp, "%s:%i",
				filenames[sym->filenr], sym->linenr);
		else
			fprintf(fp, "-command line-");
	} else
		fprintf(fp, "%9s", "??");
	fprintf(fp, "\n");

	if (fp != stdout && list_plength != 255)
		list_pln--;

	if ((list_syms == 2) && IS_LBL(sym)) {
		for (loc = sym->locals; loc; loc = loc->next) {
			if ((fp != stdout && (list_plength != 255)) && (--list_pln == 0))
				list_page("** SYMBOL TABLE **", NULL);

			fprintf(fp, "  %c%-29s %c ",
				ALPHA_CHAR, loc->name, sym_type(sym));
			fprintf(fp, "%9s          %s:%i\n",
				value_print(loc->value),
				filenames[loc->filenr], loc->linenr);
		}
	}
    }
}


void
list_save(uint32_t pc)
{
    list_pc = pc;
    list_oc = output_size;
}


void
list_close(int remov)
{
    if (list_file != NULL) {
	/* Reset printer to normal mode if needed. */
	if (opt_P && list_pwidth > 80 && list_pnr > 0)
		fprintf(list_file, LIST_CHAR_DC2);

	(void)fclose(list_file);
	list_file = NULL;

	if (remov)
		remove(list_path);
    }
}


int
list_init(const char *fn)
{
    char *p;

    /* Copy the filename and see if we need to add a filename extension. */
    strncpy(list_path, fn, sizeof(list_path) - 1);
    p = strrchr(list_path, '/');
#ifdef _WIN32
    if (p == NULL)
        p = strrchr(list_path, '\\');
#endif
    if (p == NULL)
        p = list_path;
    p = strrchr(p, '.');
    if (p != NULL)
        p++;
    if (p == NULL) {
        /* No filename extension - use .lst as a default! */
        strcat(list_path, ".lst");
    }

    list_file = fopen(list_path, "w");
    if (list_file == NULL)
	return 0;

    list_lnr = 1;
    list_pnr = list_pln = 0;
    list_pc = list_oc = 0;
    list_title = NULL;
    list_subttl = NULL;

    return 1;
}
