/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		A simple but reasonably useful assembler for the 6502.
 *
 * Usage:	vasm [-dCFqsTvPV] [-p processor] [-l fn] [-o fn] [-Dsym[=val]] file ...
 *
 * Version:	@(#)main.c	1.0.10	2023/06/15
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
#include <ctype.h>
#include <setjmp.h>
#if defined(__APPLE__) && defined(__MACH__)
 /* Apple OSX and iOS (Darwin). */
# include <TargetConditionals.h>
#  if TARGET_IPHONE_SIMULATOR == 1
  /* iOS in Xcode simulator */
# define APP_PLATFORM	"iOS"
# elif TARGET_OS_IPHONE == 1
  /* iOS */
# define APP_PLATFORM	"iOS"
# elif TARGET_OS_MAC == 1
  /* macOS */
# define APP_PLATFORM	"macOS"
# endif
#else
# ifdef _WIN32
#  define APP_PLATFORM	"Windows"
# else
#  define APP_PLATFORM	"Linux"		// any *nix, really
# endif
#endif
#ifndef _MSC_VER
# include <getopt.h>
#endif
#include "global.h"
#include "target.h"
#include "version.h"


int		opt_d,		// set DEBUG env variable to enable debug
		opt_C,		// if true, do case-insensitive symbol names
		opt_F,		// if true, perform autofill with .org
		opt_P,		// enable Printer mode
		opt_q,		// be very quiet
		opt_v;		// more verbose
char		myname[64],	// my name
		version[128];	// my full version string


#ifdef _MSC_VER
extern int	getopt(int ac, char *av[], const char *),
		optind, opterr;
extern char	*optarg;
#endif


/* Define a symbol from the command line. */
static void
do_define(char *str)
{
    char id[ID_LEN];
    char **p = &str;
    value_t v = { 0 };

    ident(p, id);
    if (**p == '=') {
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

    define_variable(id, v, 0);
}


static void
init_symbols(void)
{
    char temp[128];
    uint32_t i;

    /* Indicate "builtin" symbol. */
    line = -1;

    do_define("__VASM__");
    i = ((APP_VER_MAJOR << 24) | (APP_VER_MINOR << 16) | \
	 (APP_VER_REV << 8) | APP_VER_PATCH);
    sprintf(temp, "__VASM_VER__=%i", i);
    do_define(temp);

    /* Set for "commandline" symbols. */
    line = 0;
}


static void
usage(const char *prog)
{
    printf("Usage: %s [-dCFPqsTvV] [-p processor] [-l fn] [-o fn] [-Dsym[=val]] file ...\n", prog);

    exit(1);
    /*NOTREACHED*/
}


static void
banner(void)
{
    printf("%s %s\nCopyright 2023 Fred N. van Kempen, <waltje@varcem.com>\n",
						APP_TITLE, version);
#ifdef _WIN32
    printf("Copyright 2022,2023 Bernd B”ckmann, <bernd@varcem.com>\n\n");
#else
    printf("Copyright 2022,2023 Bernd B\303\266ckmann, <bernd@varcem.com>\n\n");
#endif
}


int
main(int argc, char *argv[])
{
    char *out_name, *lst_name;
    char *ttext;
    int c, opt_s;
    size_t size;
    int errors = 0;

    /* Set option defaults. */
#ifdef _DEBUG
    opt_d = (getenv("DEBUG") != NULL);
#endif
    opt_C = 0;
    opt_F = 1;
    opt_P = opt_s = 0;
    opt_q = opt_v = 0;
    out_name = lst_name = NULL;
    filenames_idx = -1;			// this indicates "command line"
    radix = RADIX_DEFAULT;

    /* Create any pre-defined symbols. */
    init_symbols();

    /* Create a version string. */
    sprintf(myname, "%s", APP_NAME);
    sprintf(version, "version %s (%s, %s)",
		APP_VERSION, APP_PLATFORM, STR(ARCH));

    opterr = 0;
    while ((c = getopt(argc, argv, "dCD:Fl:o:Pp:qsTvV")) != EOF) switch(c) {
	case 'C':	// toggle list-offset display (disabled)
		opt_C ^= 1;
		break;

	case 'D':	// define symbol
		do_define(optarg);
		break;

	case 'd':	// debug mode (disabled)
		opt_d ^= 1;
		break;

	case 'F':	// auto-fill for .org (enabled)
		opt_F ^= 1;
		break;

	case 'l':	// set listing file name (none)
		lst_name = optarg;
		break;

	case 'o':	// set output file name (none)
		out_name = optarg;
		break;

	case 'P':	// enable Printer mode
		opt_P ^= 1;
		break;

	case 'p':	// processor name
		if (! set_cpu(optarg, 1)) {
			fprintf(stderr, "Unknown processor '%s'.\n", optarg);
			return 1;
		}
		break;

	case 'q':	// be very quiet during operation (disabled)
		opt_q ^= 1;
		break;

	case 's':	// show (or dump) the symbol table
		opt_s ^= 1;
		list_set_syms(opt_s << 1);	// FULL or OFF
		break;

	case 'T':	// list all supported targets
		banner();
		printf("These are the supported target devices:\n\n");
		trg_list();
		exit(EXIT_SUCCESS);
		/*NOTREACHED*/

	case 'v':	// level of verbosity (lowest)
		opt_v++;
		break;

	case 'V':	// just show version and exit
		banner();
		exit(EXIT_SUCCESS);
		/*NOTREACHED*/

	default:
		usage(argv[0]);
		/*NOTREACHED*/
    }

    /* Say hello. */
    if (! opt_q)
	banner();

    /* We need at least one filename argument. */
    if (optind == argc)
	usage(argv[0]);

    /* Create output file. */
    if (! output_open(out_name)) {
	errors = 1;
	goto ret0;
    }

    /* Create a listing file if requested. */
    if ((lst_name != NULL) && !list_init(lst_name)) {
	fprintf(stderr, "Listing file '%s' could not be created!\n", lst_name);
	errors = 1;
	goto ret0;
    }

    /* Read all input files into our buffer. */
    text = NULL;
    size = 0;
    filenames_idx = 0;
    while (optind < argc) {
	if (! file_read(argv[optind], &text, &size)) {
		fprintf(stderr, "Error loading file %s\n", argv[optind]);
		errors = 1;
		goto ret0;
	}

	file_add(argv[optind++], 1, text, size);
    }
    text_len = size;

    /* Perform Pass 1. */
    ttext = text;
    errors = pass(&ttext, 1);
    if (errors)
	goto ret1;

    /* Perform Pass 2. */
    ttext = text;
    errors = pass(&ttext, 2);
    if (errors)
	goto ret2;

    /* Dump the symbols, if enabled. */
    list_symbols();

ret2:
    list_close();

ret1:
//    if (text != NULL)
//	free(text);

    sym_free(NULL);

ret0:
    if ((c = output_close(errors)) <= 0) {
	fprintf(stderr, "error writing output file %s\n", out_name);
	errors = 1;
    } else {
	if (! opt_q)
		printf("Generated %i bytes of output.\n", c);
    }

    if (errors) {
	if (lst_name != NULL)
		(void)remove(lst_name);

	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
