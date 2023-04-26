/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle reading source input. Care must be taken (especially
 *		on Windows systems, where we seem to have some issues with
 *		the "fread" function on text files) to properly read data
 *		from them when opened as a text file.
 *
 * Version:	@(#)input.c	1.0.4	2023/04/26
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


/*
 * The text variable holds all the assembler source.
 *
 * The main assembler file and all files included are joined by an EOF (0x1A)
 * character and stored as a whole in the text variable. Binary zero
 * marks the end of the assembler text.
 */
char	*text = NULL;			// holds the assembler source
int	text_len;			// total length of the source

/*
 * The filenames variable stores the filenames of all included files.
 * When parsing the source the filenames_idx variable is incremented
 * when en EOF character is encountered, and the line counter variable is
 * set to filelines[filenames_idx], current filename is set to
 * filenames[filenames_idx].
 */
char	*filenames[MAX_FILENAMES];
int	filelines[MAX_FILENAMES];
int	filenames_idx;
int	filenames_len;


/* Determine the "cooked" size (in characters) of a text file. */
size_t
file_size(const char *fn)
{
    size_t size;
    FILE *fp;
    int c;

    /* Open the file in text mode. */
    if ((fp = fopen(fn, "r")) == NULL)
	error(ERR_OPEN, fn);

    /* Text mode, so read file, counting characters. */
    size = 0;
    while (!feof(fp) && !ferror(fp)) {
	if ((c = fgetc(fp)) == EOF)
		break;
	if (c == '\r')
		continue;
	else
		size++;
    }

    /* File can be closed now. */
    (void)fclose(fp);

    return size;
}


int
file_read_buf(const char *fn, char *bufp)
{
    char *ptr;
    FILE *fp;
    int c;

    /* Open the file in text mode. */
    if ((fp = fopen(fn, "r")) == NULL)
	return 0;

    /* Now read the file's contents into the buffer. */
    ptr = bufp;
    while (!feof(fp) && !ferror(fp)) {
	if ((c = fgetc(fp)) == EOF)
		break;
	if (c == '\r')
		continue;
	*ptr++ = c;
    }
    *ptr = '\0';

    /* File can be closed now. */
    (void)fclose(fp);

    /* Return the buffer size. */
    return (int)(ptr - bufp);
}


int
file_read(const char *fn, char **pp, size_t *sizep)
{
    char *ptr;
    size_t size;
    FILE *fp;
    int c;

    /* Open the file in text mode. */
    if ((fp = fopen(fn, "r")) == NULL)
	return 0;

    /* Text mode, so read file, counting characters. */
    size = 0;
    while (!feof(fp) && !ferror(fp)) {
	if ((c = fgetc(fp)) == EOF)
		break;
	if (c == '\r')
		continue;
	else
		size++;
    }
    fseek(fp, 0, SEEK_SET);

    /* Allocate a buffer for the contents. */
    if (*pp == NULL) {
	/* No buffer yet, just allocate one. */
	*pp = malloc(size + 1);				// +1 for NUL at end
	ptr = *pp;
    } else {
	/* We already have a buffer, so add to it. */
	*pp = realloc(*pp, *sizep + 1 + size + 1);	// +1 for EOF inbetween
	ptr = *pp + *sizep;
	*ptr++ = EOF_CHAR;				// insert EOF
    }
    ptr[size] = '\0';					// terminate buffer
    *sizep += size;

    if (*pp == NULL) {
	(void)fclose(fp);
	return 0;
    }

    /* Now read the file's contents into the buffer. */
    while (!feof(fp) && !ferror(fp)) {
	if ((c = fgetc(fp)) == EOF)
		break;
	if (c == '\r')
		continue;
	*ptr++ = c;
    }
    *ptr = '\0';

    /* File can be closed now. */
    (void)fclose(fp);

    return 1;
}


void
file_add(const char *name, int linenr, const char *str, size_t size)
{
    int c = filenames_idx;

    filenames[c] = strdup(name);
    filelines[c] = 1;
//  filetexts[c] = str;
//  filesizes[c] = size;

    filenames_idx++;
    filenames_len++;
}
