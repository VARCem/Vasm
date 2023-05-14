/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the writing of output data.
 *
 * **NOTE**	This is far from 'correct' right now. We perform output in
 *		a single block right after assembly, but for non-binary
 *		output formats, this is wrong. The right way to do this is
 *		to "start" the output file as needed, and then, as we move
 *		on in the assembly, output data as needed. This also allows
 *		for the proper use of the ".org" directive, as this then
 *		merely changes the 'load address' of the following bytes.
 *
 * Version:	@(#)output.c	1.0.5	2023/05/12
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef USE_IHEX
# include <ihex.h>
#endif
#ifdef USE_SREC
# include <srec.h>
#endif
#include "global.h"
#include "error.h"


uint8_t		*code = NULL;		// holds the emitted code
uint32_t	code_base = 0;		// our load (base) address

static FILE	*out_file;		// output file


/* Set our load (base) address. */
void
emit_addr(uint32_t addr)
{
    code_base = addr;
}


/* Copy a string (of characters) to the output. */
int
emit_str(const char *p, int len, int pass)
{
    uint16_t i;

    if (pass == 2) {
	for (i = 0; i < len; i++)
		code[oc + i] = (uint8_t)*p++;
    }

    oc += len;

    return len;
}


/* Copy a single byte to the output. */
int
emit_byte(uint8_t b, int pass)
{
    if (pass == 2)
	code[oc] = b;

    oc++;

    return 1;
}


/* Copy a single word to the output. */
int
emit_word(uint16_t w, int pass)
{
    if (pass == 2) {
	/* We use little-endian format here. */
	code[oc] = w & 0xff;
	code[oc + 1] = w >> 8;
    }

    oc += 2;

    return 2;
}


/* Copy a single word (BE) to the output. */
int
emit_word_be(uint16_t w, int pass)
{
    if (pass == 2) {
	/* We use big-endian format here. */
	code[oc] = w >> 8;
	code[oc + 1] = w & 0xff;
    }

    oc += 2;

    return 2;
}


/* Copy a single double-word to the output. */
int
emit_dword(uint32_t w, int pass)
{
    if (pass == 2) {
	/* We use little-endian format here. */
	code[oc] = w & 0xff;
	code[oc + 1] = (w >> 8) & 0xff;
	code[oc + 2] = (w >> 16) & 0xff;
	code[oc + 3] = (w >> 24) & 0xff;
    }

    oc += 2;

    return 4;
}


#ifdef USE_IHEX
void
ihex_flush_buffer(ihex_t *ihex, char *buffer, char *eptr)
{
    *eptr = '\0';

    (void)fputs(buffer, out_file);
}
#endif


//FIXME: we should use out_name, and update that if we change it here!
int
save_code(const char *fn, const uint8_t *data, int len)
{
    char name[1024], *p;
#ifdef USE_IHEX
    ihex_t ihex;
    ihex_count_t count;
    uint8_t *ptr;
#endif
    int fmt, i;

    /* Copy the filename and determine the desired format. */
    strncpy(name, fn, sizeof(name) - 1);
    p = strrchr(name, '/');
#ifdef _WIN32
    if (p == NULL)
	p = strrchr(name, '\\');
#endif
    if (p == NULL)
	p = name;
    p = strrchr(p, '.');
    if (p != NULL)
	p++;

    fmt = 1;
    if (p == NULL) {
	/* No filename extension - use .bin as a default! */
	strcat(name, ".bin");
    } else if (! strcasecmp(p, "hex")) {
	fmt = 2;
    } else if (!strcasecmp(p, "srec") || !strcasecmp(p, "s19")) {
	fmt = 3;
    }

    i = 0;
    switch (fmt) {
	case 1:		// raw binary
		out_file = fopen(fn, "wb");
		if (out_file == NULL)
			break;
		i = fwrite(data, len, 1, out_file);
		(void)fclose(out_file);
		break;

	case 2:		// intel hex
#ifdef USE_IHEX
		out_file = fopen(fn, "w");
		if (out_file == NULL)
			break;
		ihex_init(&ihex);
//		ihex_write_at_address(&ihex, pc);
		ihex_set_output_line_length(&ihex,
					    IHEX_DEFAULT_OUTPUT_LINE_LENGTH);
		ptr = code;
        	while (oc > 0) {
			count = (oc > 1024) ? 1024 : oc;
			ihex_write_bytes(&ihex, ptr, count);
			ptr += count;
			oc -= count;
        	}

		if (sa != 0)
			ihex_write_start_address(&ihex, sa);

        	ihex_end_write(&ihex);
		(void)fclose(out_file);
		i = 1;
#else
		error(ERR_NO_FMT, p);
#endif
		break;

	case 3:		// motorola SRecord
		i = 0;
		error(ERR_NO_FMT, p);
    }

    out_file = NULL;

    /* If this did not work, let them know. */
    if ((i == 0) && (pc != 0))
	return 0;

    return 1;
}
