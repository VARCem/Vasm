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
 *		The output module handles output from the assembler into a
 *		file that can be used by the target device. This can be in
 *		the form of a raw-binary "object code" file, or it can be
 *		one of the several text-formatted files, such as Intel Hex
 *		or Motorola SRecord.
 *
 *		The biggest difference between the "formatted" files and a
 *		raw binary file is, that the latter usually cannot handle
 *		any "skips" in the object file, as implemented by the .ORG
 *		assembler directive. Formatted files will simply indicate
 *		a change of load address for the data following, but in a
 *		binary file, we cannot do that.
 *
 *		To "fix" this, we will assume that if we detect a skip in
 *		load address, we "fill out" the gap with bytes of some
 *		value - usually this is either 0x00 or 0xff (the latter is
 *		often used for images that have to go into ROM.) This of
 *		course creates a potentially large "air bubble" in these
 *		files, so if this was not wanted, the global -F option
 *		(or the .NOFILL assembler directive) can be used to disable
 *		this behavior.
 *
 * FIXME:	We probably should merge the little/big endian functions
 *		into one, and have the backends select the proper mode for
 *		them at runtime.
 *
 * Version:	@(#)output.c	1.0.6	2023/06/16
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


#define IHEX_MAX	32		// max #bytes per line
#define SREC_MAX	255		// max #bytes per line


uint32_t	output_size;		// total #bytes in output buffer
uint8_t		*output_buff;		// output data buffer

static char	out_path[1024];		// actual output filename
static int	out_format;
static FILE	*out_file;		// output file
static int	out_count,		// current #bytes in buffer
		out_max;		// max #bytes in buffer
static uint32_t	out_base;		// our load (base) address
static uint8_t	*out_line;		// line output buffer
static int8_t	out_orgdone;		// has a .org been performed?


static void
out_flush(int force)
{
    char temp[1024], *p;
    int base, i, k, sum;

    if (out_file == NULL || out_format == 0)
	return;

    if ((out_count < out_max) && !force)
	return;

    base = 0;
    while (out_count > 0) {
	p = temp;
	sum = 0;

	/* Create the line header. */
	k = (out_count > out_max) ? out_max : out_count;

	if (out_format == 1)	// Intel Hex
		p += sprintf(p, ":%02X%02X%02X00",
			k, (out_base >> 8), (out_base & 0xff));
	else			// Motorola SRec
		p += sprintf(p, "S1%02X%02X%02X",
			k, (out_base >> 8), (out_base & 0xff));

	sum = k;
	sum += (out_base & 0xff);
	sum += (out_base >> 8);

	/* Add the data bytes (payload.) */
	for (i = 0; i < k; i++) {
		p += sprintf(p, "%02X", out_line[base + i]);
		sum += out_line[base + i];
	}

	/* Calculate the 1-complement checksum byte. */
	sum = (~sum & 0xff);

	/* Intel Hex uses 2-complement, Moto SRec uses 1-complement. */
	if (out_format == 1)
		sum++;

	sprintf(p, "%02X", sum & 0xff);

	fprintf(out_file, "%s\n", temp);

	base += k;
	out_base += k;
	out_count -= k;
    }
}


/* Store one byte of data. */
static void
out_store(uint8_t b, int pass)
{
    /* OK, one more byte in the buffer. */
    output_size++;

    if (out_format == 0) {
	/* Note that writing output implies we set an origin. */
	out_orgdone = 1;

	out_base++;
    }

    /* If not in Pass 2, we're all done. */
    if (pass != 2)
	return;

    /* Store byte in output buffer. */
    output_buff[output_size - 1] = b;

    if (out_max > 0) {
	/*
	 * This is an ASCII text format with a line buffer.
	 *
	 * Do we have room in the output line?
	 */
	if (out_count >= out_max) {
		/* No, we have to flush first. */
		out_flush(1);
	}

	out_line[out_count++] = b;
    } else {
	/* Binary format, just write it out. */
	fputc(b, out_file);
    }
}


/*
 * Create the output file in the requested format.
 *
 * The format is set either by the filename extension (with most
 * of the usual formats, this works just fine), or by explicitly
 * setting the formatname: prefix to the filename, such as:
 *
 *    ihex:testfile.txt
 *
 * which then, even though the extension is "txt", will be se to
 * Intel Hex format because of the prefix.
 */
int
output_open(const char *fn)
{
    char *p, *pfx, *s;

    /* Initialize. */
    out_orgdone = 0;
    out_file = NULL;
    output_buff = out_line = NULL;
    output_reset();

    /* Check for prefixes, overriding the extension. */
    strncpy(out_path, fn, sizeof(out_path) - 1);
    pfx = strchr(out_path, ':');
    if (pfx != NULL) {
	*pfx = '\0';
	s = ++pfx;
	pfx = out_path;
    } else
	s = out_path;

    /* Determine the desired format based on suffix. */
    p = strrchr(s, '/');
#ifdef _WIN32
    if (p == NULL)
	p = strrchr(s, '\\');
#endif
    if (p == NULL)
	p = s;
    p = strrchr(p, '.');
    if (p != NULL)
	p++;

    /* If no suffix at all, attach one. */
    if (p == NULL) {
	p = "bin";
	strcat(s, ".bin");
    }

    /* But.. prefixes override a suffix. */
    if (pfx != NULL)
	p = pfx;

    if (!strcasecmp(p, "ihex") || !strcasecmp(p, "hex")) {
	out_max = IHEX_MAX;
	out_format = 1;
	out_file = fopen(s, "w");
    } else if (!strcasecmp(p, "srec") || !strcasecmp(p, "s19")) {
	out_max = SREC_MAX;
	out_format = 2;
	out_file = fopen(s, "w");
    } else {
	/* No known format name, assume raw-binary. */
	out_max = out_format = 0;

	/* If this was a prefix: we did not recognize it. */
	if (p == pfx) {
		fprintf(stderr, "Error: %s (%s)\n", err_msgs[ERR_NO_FMT], p);
		return 0;
	}

	/* All good, create the file. */
	out_file = fopen(s, "wb");
    }

    if (out_file == NULL) {
	fprintf(stderr, "Error: %s (%s)\n", err_msgs[ERR_CREATE], s);
	return 0;
    }

    /* Allocate line buffer if needed. */
    if (out_max > 0) {
	out_line = malloc(out_max);
	if (out_line == NULL) {
		fprintf(stderr, "Error: %s (%s)\n", err_msgs[ERR_MEM], "line buffer");
		return 0;
	}
	memset(out_line, 0x00, out_max);
    }

    return 1;
}


/*
 * Close the currently-open output file.
 *
 * If we still have unwritten data, flush that out before
 * closing the file. If the file has to be removed again,
 * usually because of some error, do that after closing.
 */
int
output_close(int remov)
{
    if (out_file == NULL)
	return 0;

    /* Flush any buffered data. */
    out_flush(1);

    if (out_format == 1) {
	/* Write the EOF record. */
	fprintf(out_file, ":00000001FF\n");
    }

    (void)fclose(out_file);
    out_file = NULL;

    if (remov)
	remove(out_path);

    if (out_line != NULL) {
	free(out_line);
 	out_line = NULL;
    }
    if (output_buff != NULL) {
	free(output_buff);
	output_buff = NULL;
    }

    return output_size;
}


void
output_reset(void)
{
    out_base = 0;

    if (output_size > 0) {
	/* This is Pass 2, allocate buffer. */
	output_buff = malloc(output_size);
	if (output_buff == NULL)
		error(ERR_MEM, "output buffer");
	memset(output_buff, 0x00, output_size);
    }

    output_size = out_count = 0;
}


/*
 * Set our load (base) address.
 *
 * If we currently have buffered data, flush that out first.
 *
 * Then, set the (new) load address.
 */
void
output_addr(uint32_t addr, int pass)
{
    if (out_format == 0) {
	/*
	 * Optionally, "fill out" the space with $00 bytes.
	 *
	 * Note that we do NOT do this the first time the origin
	 * is changed (from being $0000) since that would not be
	 * sensible. We only do it if we change it on the fly.
	 */
	if (opt_F) {
		if (out_orgdone) {
			/* Not the first .ORG, fill. */
			while (out_base < addr)
				out_store(0x00, pass);
		} else {
			/* First .ORG, ignore. */
			out_orgdone = 1;
		}
	}
    } else {
	/* Flush pending data for text formats. */
	if (pass == 2)
		out_flush(1);
    }

    /* Either way, set the new origin. */
    out_base = addr;
}


/*
 * Set the (optional) start address of the generated code.
 */
void
output_start(uint32_t addr, int pass)
{
    char temp[128], *p;
    int sum;

    /* Flush any buffered data. */
    if (pass != 2)
	return;

    out_flush(1);

    if (out_format != 0) {
	p = temp;

	if (out_format == 1)	// Intel Hex
		p += sprintf(p, ":04000005%02X%02X%02X%02X",
			(addr>>24), (addr>>16), (addr>>8), (addr&0xff));
	else			// Motorola SRec
		p += sprintf(p, "S904%02X%02X%02X%02X",
			(addr>>24), (addr>>16), (addr>>8), (addr&0xff));

	sum = 4 + 5;
	sum += (addr & 0xff);
	sum += (addr >> 8);
	sum += (addr >> 16);
	sum += (addr >> 24);

	/* Calculate the 1-complement checksum byte. */
	sum = (~sum & 0xff);

	/* Intel Hex uses 2-complement, Moto SRec uses 1-complement. */
	if (out_format == 1)
		sum++;

	sprintf(p, "%02X", sum & 0xff);

	fprintf(out_file, "%s\n", temp);
    }
}


/* Copy a string (of characters) to the output. */
void
emit_str(const char *p, int len, int pass)
{
    while (len-- > 0)
	out_store((uint8_t)*p++, pass);
}


/* Copy a single byte to the output. */
void
emit_byte(uint8_t b, int pass)
{
    out_store(b, pass);
}


/* Copy a single word to the output. */
void
emit_word(uint16_t w, int pass)
{
    /* We use little-endian format here. */
    out_store(w & 0xff, pass);
    out_store(w >> 8, pass);
}


/* Copy a single word (BE) to the output. */
void
emit_word_be(uint16_t w, int pass)
{
    /* We use big-endian format here. */
    out_store(w >> 8, pass);
    out_store(w & 0xff, pass);
}


/* Copy a single double-word to the output. */
void
emit_dword(uint32_t w, int pass)
{
    /* We use little-endian format here. */
    out_store(w & 0xff, pass);
    out_store(w >> 8, pass);
    out_store(w >> 16, pass);
    out_store(w >> 24, pass);
}


/* Copy a single double-word (BE) to the output. */
void
emit_dword_be(uint32_t w, int pass)
{
    /* We use big-endian format here. */
    out_store(w >> 24, pass);
    out_store(w >> 16, pass);
    out_store(w >> 8, pass);
    out_store(w & 0xff, pass);
}
