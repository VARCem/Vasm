/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the Signetics SCN2650 device target.
 *
 *		This module ended up being quite messy, but mostly because
 *		we tried to recognize syntaxes from several assemblers out
 *		there. For the most part, this works.
 *
 * Version:	@(#)scn2650.c	1.0.1	2023/05/15
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
#include <ctype.h>
#include "../global.h"
#include "../error.h"
#include "../target.h"


typedef enum {
    AM_IMP = 1,			// (none, implied)
    AM_COND,			// EQ,LT,GT,UN
    AM_REG,			// R0,R1,R2,R3
    AM_IMM,			// #$12
    AM_IMM_PSL,			// II,OVF etc
    AM_REL,			// LABEL
    AM_ABS,			// $1234
    AM_BRANCH			// COND,LABEL
} trg_modes_t;

typedef enum {
    ERR_NOTIMP = ERR_MAXERR,	// "instruction not implemented.."
    ERR_AM,			// "invalid addressing mode"
    ERR_REG,			// "invalid register"
    ERR_REG0,			// "R0 not allowed"
    ERR_COND,			// "invalid condition"
    ERR_ILLAM,			// "malformed addressing mode"
    ERR_RELRNG,			// "relative target out of range"

    ERR_MAXTRG			// last local error message
} trg_errors_t;


#define CPU_0	0x00			// original SCN2650I
#define CPU_A	0x01			// standard SCN2650AN
#define CPU_B	0x02			// standard SCN2650BN


typedef struct opcode {
    char	mn[5];
    uint8_t	cpu;
    uint8_t	opcode;
    int8_t	am;
} opcode_t;


static const opcode_t opcodes[] = {
    { "ADDA",	CPU_0, 0x8c, AM_ABS	},
    { "ADDI",	CPU_0, 0x84, AM_IMM	},
    { "ADDR",	CPU_0, 0x88, AM_REL	},
    { "ADDZ",	CPU_0, 0x80, AM_REG	},
    { "ANDA",	CPU_0, 0x4c, AM_ABS	},
    { "ANDI",	CPU_0, 0x44, AM_IMM	},
    { "ANDR",	CPU_0, 0x48, AM_REL	},
    { "ANDZ",	CPU_0, 0x40, AM_REG	},	// R may not be 0 !
    { "BCFA",	CPU_0, 0x9c, AM_BRANCH	},
    { "BCFR",	CPU_0, 0x98, AM_BRANCH	},
    { "BCTA",	CPU_0, 0x1c, AM_BRANCH	},
    { "BCTR",	CPU_0, 0x18, AM_BRANCH	},
    { "BDRA",	CPU_0, 0xfc, AM_BRANCH	},
    { "BDRR",	CPU_0, 0xf8, AM_BRANCH	},
    { "BIRA",	CPU_0, 0xdc, AM_BRANCH	},
    { "BIRR",	CPU_0, 0xd8, AM_BRANCH	},
    { "BRNA",	CPU_0, 0x5c, AM_BRANCH	},
    { "BRNR",	CPU_0, 0x58, AM_BRANCH	},
    { "BSFA",	CPU_0, 0xbc, AM_BRANCH	},
    { "BSFR",	CPU_0, 0xb8, AM_BRANCH	},
    { "BSNA",	CPU_0, 0x7c, AM_BRANCH	},
    { "BSNR",	CPU_0, 0x78, AM_BRANCH	},
    { "BSTA",	CPU_0, 0x3c, AM_BRANCH	},
    { "BSTR",	CPU_0, 0x38, AM_BRANCH	},
    { "BSXA",	CPU_0, 0xbf, AM_BRANCH	},
    { "BXA",	CPU_0, 0x9f, AM_BRANCH	},
    { "CMPA",	CPU_0, 0xec, AM_ABS	},	// alternative name for COM
    { "CMPI",	CPU_0, 0xe4, AM_IMM	},
    { "CMPR",	CPU_0, 0xe8, AM_REL	},
    { "CMPZ",	CPU_0, 0xe0, AM_REG	},
    { "COMA",	CPU_0, 0xec, AM_ABS	},	// normal name
    { "COMI",	CPU_0, 0xe4, AM_IMM	},
    { "COMR",	CPU_0, 0xe8, AM_REL	},
    { "COMZ",	CPU_0, 0xe0, AM_REG	},
    { "CPSL",	CPU_0, 0x75, AM_IMM_PSL	},
    { "CPSU",	CPU_0, 0x74, AM_IMM_PSL	},
    { "DAR",	CPU_0, 0x94, AM_REG	},
    { "EORA",	CPU_0, 0x2c, AM_ABS	},
    { "EORI",	CPU_0, 0x24, AM_IMM	},
    { "EORR",	CPU_0, 0x28, AM_REL	},
    { "EORZ",	CPU_0, 0x20, AM_REG	},
    { "HALT",	CPU_0, 0x40, AM_IMP	},
    { "IORA",	CPU_0, 0x6c, AM_ABS	},
    { "IORI",	CPU_0, 0x64, AM_IMM	},
    { "IORR",	CPU_0, 0x68, AM_REL	},
    { "IORZ",	CPU_0, 0x60, AM_REG	},
    { "LDPL",	CPU_B, 0x10, AM_ABS	},	// 2650B only
    { "LODA",	CPU_0, 0x0c, AM_ABS	},
    { "LODI",	CPU_0, 0x04, AM_IMM	},
    { "LODR",	CPU_0, 0x08, AM_REL	},
    { "LODZ",	CPU_0, 0x00, AM_REG	},
    { "LPSL",	CPU_0, 0x93, AM_IMP	},
    { "LPSU",	CPU_0, 0x92, AM_IMP	},
    { "NOP",	CPU_0, 0xc0, AM_IMP	},
    { "PPSL",	CPU_0, 0x77, AM_IMM_PSL	},
    { "PPSU",	CPU_0, 0x76, AM_IMM_PSL	},
    { "REDC",	CPU_0, 0x30, AM_REG	},
    { "REDD",	CPU_0, 0x70, AM_REG	},
    { "REDE",	CPU_0, 0x54, AM_IMM	},
    { "RETC",	CPU_0, 0x14, AM_COND	},
    { "RETE",	CPU_0, 0x34, AM_COND	},
    { "RRL",	CPU_0, 0xd0, AM_REG	},
    { "RRR",	CPU_0, 0x50, AM_REG	},
    { "SPSL",	CPU_0, 0x13, AM_IMP	},
    { "SPSU",	CPU_0, 0x12, AM_IMP	},
    { "STPL",	CPU_B, 0x11, AM_ABS	},	// 2650B only
    { "STRA",	CPU_0, 0xcc, AM_ABS	},
    { "STRR",	CPU_0, 0xc8, AM_REL	},
    { "STRZ",	CPU_0, 0xc0, AM_REG	},	// R may not be 0 !
    { "SUBA",	CPU_0, 0xac, AM_ABS	},
    { "SUBI",	CPU_0, 0xa4, AM_IMM	},
    { "SUBR",	CPU_0, 0xa8, AM_REL	},
    { "SUBZ",	CPU_0, 0xa0, AM_REG	},
    { "TMI",	CPU_0, 0xf4, AM_IMM	},
    { "TPSL",	CPU_0, 0xb5, AM_IMM_PSL	},
    { "TPSU",	CPU_0, 0xb4, AM_IMM_PSL	},
    { "WRTC",	CPU_0, 0xb0, AM_REG	},
    { "WRTD",	CPU_0, 0xf0, AM_REG	},
    { "WRTE",	CPU_0, 0xd4, AM_IMM	},
    { "ZBRR",	CPU_0, 0x9b, AM_BRANCH	},
    { "ZBSR",	CPU_0, 0xbb, AM_BRANCH	}
};


static const char *err_msg[ERR_MAXTRG - ERR_MAXERR] = {
    "instruction not implemented",
    "invalid addressing mode",
    "invalid register",
    "register R0 not allowed",
    "invalid condition",
    "malformed addressing mode",
    "relative target out of range"
};


static int
get_bits(char **p)
{
    char id[ID_LEN], *pp;
    char *pt = *p;
    int ret = -1;

    /* Convert the argument to uppercase. */
    pp = id;
    while (!IS_EOL(**p) && !IS_SPACE(**p)) {
        *pp++ = (char)toupper(**p);
	(*p)++;
    }
    *pp = '\0';

    if (! strncmp(id, "SP", 2)) {
	ret = atoi(&id[2]);
	return ret;
    }
    if (! strcmp(id, "II"))
	return 0x20;
    if (!strcmp(id, "F") || !strcmp(id, "FLAG"))
	return 0x40;
    if (!strcmp(id, "S") || !strcmp(id, "SENSE"))
	return 0x80;

    if (! strcmp(id, "C"))
	return 0x01;
    if (! strcmp(id, "COM"))
	return 0x02;
    if (! strcmp(id, "OVF"))
	return 0x04;
    if (! strcmp(id, "WC"))
	return 0x08;
    if (! strcmp(id, "RS"))
	return 0x10;
    if (! strcmp(id, "IDC"))
	return 0x20;
    if (! strncmp(id, "CC", 2)) {
	ret = atoi(&id[2]);
	return ret;
    }

    *p = pt;

    return ret;
}


static int
get_condition(char **p)
{
    char id[ID_LEN], *pp;
    value_t v;
    int ret;

    /* Convert the argument to uppercase. */
    pp = id;
    while (!IS_EOL(**p) && !IS_SPACE(**p) && **p != ',') {
        *pp++ = (char)toupper(**p);
	(*p)++;
    }
    *pp = '\0';

    if (!strcmp(id, "EQ") || !strcmp(id, ".EQ."))
	return 0;
    if (!strcmp(id, "GT") || !strcmp(id, ".GT."))
	return 1;
    if (!strcmp(id, "LT") || !strcmp(id, ".LT."))
	return 2;
    if (!strcmp(id, "UN") || !strcmp(id, ".UN."))
	return 3;

    /* Hmm, getting desperate here.. */
    if (**p == '#') {
	(*p)++;
	skip_white(p);
    }

    /* Wow, they are using an expression for the condition.. */
    v = expr(p);
    ret = (int)v.v & 0x03;

    return ret;
}


static int
get_register(char **p)
{
    char id[ID_LEN];
    int reg;

    /* Convert the register name to uppercase. */
    ident_upcase(p, id);
    if (id[0] != 'R')
	return -1;

    reg = atoi(&id[1]);
    if (reg < 0 || reg > 3)
	return -1;

    return reg;
}


/*
 * Handle the Implied mode.
 *
 * byte1 = opcode
 *
 * HALT
 */
static int
op_imp(char **p, int pass, const opcode_t *instr)
{
    emit_byte(instr->opcode, pass);

    return 1;
}


/*
 * Handle the Conditional mode.
 *
 * byte1 = opcode | cond[1:0]
 *
 * RETC,cond
 */
static int
op_cond(char **p, int pass, const opcode_t *instr)
{
    int cond;

    /* Skip the comma, if we have it. */
    if (**p == ',')
	(*p)++;
    skip_white(p);

    /* Now get the register. */
    cond = get_condition(p);
    if (cond < 0)
	error(ERR_COND, NULL);

    emit_byte(instr->opcode|cond, pass);

    return 1;
}


/*
 * Handle the Register mode.
 *
 * byte1 = opcode | reg[1:0]
 *
 * LODZ  r
 */
static int
op_reg(char **p, int pass, const opcode_t *instr)
{
    int reg;

    /* Skip the comma, if we have it. */
    if (**p == ',')
	(*p)++;
    skip_white(p);

    /* Get the register. */
    reg = get_register(p);
    if (reg < 0)
	error(ERR_REG, NULL);

    /* For these, register may not be R0. */
    if (instr->opcode == 0x40 || instr->opcode == 0xc0) {
	if (reg == 0)
		error(ERR_REG0, NULL);
    }

    emit_byte(instr->opcode|reg, pass);

    return 1;
}


/*
 * Handle the Immediate mode.
 *
 * byte1 = opcode | reg[1:0]
 * byte2 = imm arg
 *
 * LODI,r value
 * LODI r, value
 */
static int
op_imm(char **p, int pass, const opcode_t *instr)
{
    value_t v;
    int reg, fmt;

    /* Skip the comma, if we have it. */
    fmt = 2;
    if (**p == ',') {
	(*p)++;
	fmt = 1;
    }
    skip_white(p);

    /* Get the register. */
    reg = get_register(p);
    if (reg < 0)
	error(ERR_REG, NULL);

    /* Skip the comma if we need one. */
    skip_white(p);
    if (fmt == 2) {
	if (**p != ',')
		error(ERR_COMMA, NULL);

	(*p)++;
	skip_white(p);
    }

    /* Some assemblers allow the '#' character here. */
    if (**p == '#')
	(*p)++;

    /* Get the (immediate) argument. */
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }

    emit_byte(instr->opcode|reg, pass);
    emit_byte((uint8_t)to_byte(v, 0).v, pass);

    return 2;
}


/*
 * Handle the Immediate mode for Processor Status.
 *
 * byte1 = opcode
 * byte2 = imm arg
 *
 * LPSU ii
 */
static int
op_imm_psl(char **p, int pass, const opcode_t *instr)
{
    value_t v;
    int arg;

    /* Get the (immediate) argument. */
    arg = get_bits(p);
    if (arg < 0) {
	if (**p == '#') {
		(*p)++;
		skip_white(p);
	}

	v = expr(p);
	if (pass == 2) {
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);
	}
	arg = (int)to_byte(v, 0).v;
    }

    /*
     * FIXME:
     *
     * If the opcode is 0x76 (PPSU), should we check for
     * the setting of bits 3 and 4, which are unused on
     * the 2650 and 2650A?
     */

    emit_byte(instr->opcode, pass);
    emit_byte(arg & 0xff, pass);

    return 2;
}


/*
 * Handle the Absolute mode.
 *
 * byte1 = opcode || reg[1:0]
 * byte2 = rel arg H
 * byte3 = rel arg L
 *
 * LODA,r value[,X[[,]+-]]  (relative)
 * LODA,r *value[,X[[,]+-]] (indirect)
 *
 * LODA r, value[,X[[,]+-]]  (relative)
 * LODA r, *value[,X[[,]+-]] (indirect)
 *
 * Note that if indexing is used, register must always be R0 !
 */
static int
op_abs(char **p, int pass, const opcode_t *instr)
{
    int ind, fmt;
    int incdec = 0;
    int idx = -1;
    int reg = 0;
    uint16_t addr;
    value_t v;

    /* LDDL and STPL do not allow registers or indexing. */
    if (instr->opcode != 0x10 && instr->opcode != 0x11) {
	/* Skip the comma, if we have it. */
	fmt = 2;
	if (**p == ',') {
		(*p)++;
		fmt = 1;
	}
	skip_white(p);

	/* Get the register. */
	reg = get_register(p);
	if (reg < 0)
		error(ERR_REG, NULL);

	/* Skip the comma if we need one. */
	skip_white(p);
	if (fmt == 2) {
		if (**p != ',')
			error(ERR_COMMA, NULL);

		(*p)++;
		skip_white(p);
	}
    }

    /*
     * Check for Indirect mode.
     *
     * Note: many assemblers use () or [] for this, and not
     *       the * prefix. Maybe implement at least [] here?
     */
    if (**p == '*' || **p == '@' || **p == '[') {
	/* Indirect mode! */
	ind = 1;
	if (**p == '[')
		ind++;
	(*p)++;
	skip_white(p);
    } else
	ind = 0;

    /* Get the (absolute) argument. */
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }

    if (instr->opcode != 0x10 && instr->opcode != 0x11) {
	/* See if we have a comma here. */
	skip_white(p);
	if (**p == ',') {
		(*p)++;
		skip_white(p);

		/*
		 * Check for auto-increase or auto-decrease.
		 *
		 * Some assemblers use ",+Rx" to indicate the
		 * auto-increment/decrement, mostly because it
		 * is done BEFORE using the value. As far as I
		 * can check, most assemblers use the ",Rx+"
		 * form, but we do both..
		 */
		if (**p == '+') {
			(*p)++;
			incdec = 1;
		} else if (**p == '-') {
			(*p)++;
			incdec = -1;
		}
	
		idx = get_register(p);

		/*
		 * Indexing means that the register automatically
		 * becomes R0, and the indexing register is set as
		 * the register in the opcode, as stated below.
		 * The datasheet does not state (explicitly) what
		 * we "do" with the register then. Some test code
		 * actually uses R0 for both, and that apparently
		 * is *NOT* to be treated as an error..
		 */
#if 0
		if (idx <= 0 || reg != 0)
#else
		if (idx < 0 || reg != 0)
#endif
			error(ERR_REG, NULL);

		/*
		 * In indexed absolute mode, the register field is
		 * is forced to be R0, and its bits are set to the
		 * index register instead.
		 */
		reg = idx;

		/* If we have not done this BEFORE the register name.. */
		if (incdec == 0) {
			/* Some assemblers allow a comma here. */
			if (**p == ',')
				(*p)++;

			/* .. check for auto-increase or auto-decrease. */
			if (**p == '+') {
				(*p)++;
				incdec = 1;
			} else if (**p == '-') {
				(*p)++;
				incdec = -1;
			}
		}
	}
    }

    if (ind == 2) {
	skip_white(p);
	if (**p != ']')
		error(ERR_ILLAM, NULL);
	(*p)++;
    }

    /* Format the address word. */
    addr = (uint16_t)v.v;
#if 0
    if (idx > 0) {
#else
    if (idx >= 0) {
#endif
	if (incdec < 0)
		addr |= 0x4000;
	else if (incdec > 0)
		addr |= 0x2000;
	else
		addr |= 0x6000;
    }
    if (ind)
	addr |= 0x8000;

    emit_byte(instr->opcode|reg, pass);
    emit_word_be(addr, pass);

    return 3;
}


/*
 * Handle the Non-Branch Relative mode.
 *
 * byte1 = opcode || reg[1:0]
 * byte2 = rel arg H
 * if (byte2 & 0x80) arg is indirect
 *
 * LODR,r value  (relative)
 * LODR,r *value (indirect)
 *
 * LODR r, value  (relative)
 * LODR r, *value (indirect)
 */
static int
op_rel(char **p, int pass, const opcode_t *instr)
{
    int ind, fmt, reg;
    uint16_t addr, pct;
    uint16_t off;
    value_t v;

    /* Skip the comma, if we have it. */
    fmt = 2;
    if (**p == ',') {
	(*p)++;
	fmt = 1;
    }
    skip_white(p);

    /* Get the register. */
    reg = get_register(p);
    if (reg < 0)
	error(ERR_REG, NULL);

    /* Skip the comma if we need one. */
    skip_white(p);
    if (fmt == 2) {
	if (**p != ',')
		error(ERR_COMMA, NULL);

	(*p)++;
	skip_white(p);
    }

    /*
     * Check for Indirect mode.
     *
     * Note: many assemblers use () or [] for this, and not
     *       the * prefix. Maybe implement at least [] here?
     */
    if (**p == '*' || **p == '@' || **p == '[') {
	/* Indirect mode! */
	ind = 1;
	if (**p == '[')
		ind++;
	(*p)++;
	skip_white(p);
    } else
	ind = 0;

    /* Get the (relative) argument. */
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }
    addr = (uint16_t)v.v;

    if (ind == 2) {
	skip_white(p);
	if (**p != ']')
		error(ERR_ILLAM, NULL);
	(*p)++;
    }

    pct = pc + 2;

    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);

	if ((addr >= pct) && ((uint16_t)(addr - pct) > 0x3f))
		error(ERR_RELRNG, NULL);
	else if ((pct > addr) && ((uint16_t)(pct - addr) > 0x40))
		error(ERR_RELRNG, NULL);
    }

    if (addr >= pct)
	off = addr - pct;
    else
	off = (uint16_t)((~0) - (pct - addr - 1));
    off &= 0x7f;

    /* Format the address word. */
    if (ind)
	off |= 0x80;

    emit_byte(instr->opcode|reg, pass);
    emit_byte(off & 0xff, pass);

    return 2;
}


/*
 * Handle the Branch mode.
 *
 * byte1 = opcode
 * byte2 = addr (rel)
 *
 * byte2 = addr H
 * byte3 = addr L
 *
 * if (byte2 & 0x80) addr is indirect
 *
 * ZBRR value
 * ZBRR *value (indirect)
 */
static int
op_branch(char **p, int pass, const opcode_t *instr)
{
    uint16_t addr, pct, off;
    int ind, arg = -1;
    int fmt = 2;
    value_t v;

    /* ZBRR, ZBSR, BXA and BXSA do not have conditions. */
    if (instr->opcode != 0x9b && instr->opcode != 0xbb &&
        instr->opcode != 0x9f && instr->opcode != 0xbf) {
	/* Get register or condition. */
	if (**p == ',') {
		(*p)++;
		skip_white(p);
	} else
		fmt = 1;

	if (**p == 'r' || **p == 'R') {
		arg = get_register(p);
		if (arg < 0)
			error(ERR_REG, NULL);
	} else
		arg = get_condition(p);
	skip_white(p);

	if (fmt == 1) {
		if (**p == ',')
			(*p)++;
	}
    }

    if (arg < 0)
	arg = 3;	/* unconditional */

    /*
     * Check for Indirect mode.
     *
     * Note: many assemblers use () or [] for this, and not
     *       the * prefix. Maybe implement at least [] here?
     */
    if (**p == '*' || **p == '@' || **p == '[') {
	/* Indirect mode! */
	ind = 1;
	if (**p == '[')
		ind++;
	(*p)++;
	skip_white(p);
    } else
	ind = 0;

    /* Get the address. */
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }
    addr = (uint16_t)v.v;

    if (ind == 2) {
	skip_white(p);
	if (**p != ']')
		error(ERR_ILLAM, NULL);
	(*p)++;
    }

    /* Bit 2 of the opcode tells us whether Absolute or Relative. */
    if (instr->opcode & 0x04) {
	/* Format the address word. */
	addr &= 0x7fff;
	if (ind)
		addr |= 0x8000;

	emit_byte(instr->opcode|arg, pass);
	emit_word_be(addr, pass);
    } else {
	/* ZBRR and ZBSR are relative to 0, others are relative to PC+2. */
	if (instr->opcode == 0x9b || instr->opcode == 0xbb)
		pct = 0x0000;
	else
		pct = pc + 2;

	if (pass == 2) {
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);

		if ((addr >= pct) && ((uint16_t)(addr - pct) > 0x3f))
			error(ERR_RELRNG, NULL);
		else if ((pct > addr) && ((uint16_t)(pct - addr) > 0x40))
			error(ERR_RELRNG, NULL);
	}

	if (addr >= pct)
		off = addr - pct;
	else
		off = (uint16_t)((~0) - (pct - addr - 1));
	off &= 0x7f;

	/* Format the address word. */
	if (ind)
		off |= 0x80;

	emit_byte(instr->opcode|arg, pass);
	emit_byte(off & 0xff, pass);
    }

    return (instr->opcode & 0x04) ? 3 : 2;
}


/* Look up a mnemonic in the table. */
static const opcode_t *
get_mnemonic(const opcode_t *table, int size, const char *p)
{
    int i, k;

    for (k = 0; k < size; k++) {
	if ((i = strcmp(table[k].mn, p)) >= 0) {
		if (i == 0)
			return &table[k];
		break;
	}
    }

   return NULL;
}


/*
 * Process one instruction.
 *
 * This gets called by the main code, once per pass. Its task is
 * to process an entire instruction line, or, at least, the part
 * of the line where the assembler thinks the "code" is. So, in
 * most cases, we get called with "p" pointing to the very start
 * of the code and we can begin parsing right away.
 *
 * If we find an error, we simply call the error() function, and
 * that will properly terminate all processing for us.
 *
 * During pass 2, we will generate any code bytes, which we emit
 * by calling the emit() function.
 */
static int
t_instr(const target_t *trg, char **p, int pass)
{
    char id[ID_LEN];
    const opcode_t *op;
    int bytes = 0;

    /* Convert the mnemonic to uppercase. */
    ident_upcase(p, id);

    /* First get instruction for given mnemonic. */
    op = get_mnemonic((const opcode_t *)trg->priv, trg->priv2, id);
    if (op == NULL)
	error(ERR_INSTR, NULL);

    /* Make sure we are allowed to use this instruction. */
    if (op->cpu > trg->flags)
	error(ERR_NOTIMP, id);

    /* Found, so get addressing mode. */
    skip_white(p);

    switch (op->am) {
	case AM_IMP:
		bytes = op_imp(p, pass, op);
		break;

	case AM_COND:
		bytes = op_cond(p, pass, op);
		break;

	case AM_REG:
		bytes = op_reg(p, pass, op);
		break;

	case AM_IMM:
		bytes = op_imm(p, pass, op);
		break;

	case AM_IMM_PSL:
		bytes = op_imm_psl(p, pass, op);
		break;

	case AM_ABS:
		bytes = op_abs(p, pass, op);
		break;

	case AM_REL:
		bytes = op_rel(p, pass, op);
		break;

	case AM_BRANCH:
		bytes = op_branch(p, pass, op);
		break;

	default:
		break;
    }

    /* Bail out if that could not be done. */
    if (bytes == 0)
	error(ERR_AM, id);

    /* Return the amount of code generated. */
    return bytes;
}


/*
 * Check to see if the identifier matches an existing mnemonic.
 *
 * The assembler will not allow the use of these "reserved words"
 * as label names.
 */
static int
t_instr_ok(const target_t *trg, const char *p)
{
    char id[ID_LEN], *ptr;
    const opcode_t *op;

    /* Convert the mnemonic to uppercase. */
    ptr = id;
    while (*p != '\0')
        *ptr++ = (char)toupper(*p++);
    *ptr = '\0';

    /* Get instruction for given mnemonic. */
    op = get_mnemonic((const opcode_t *)trg->priv, trg->priv2, id);
    if (op != NULL)
	return 1;

    return 0;
}


/* Return a local error message. */
static const char *
t_error(int err)
{
    if (err < ERR_MAXTRG)
	return err_msg[err - ERR_MAXERR];

    return "??";
}


const target_t t_2650 = {
    "2650", CPU_0,
    "Signetics SCN2650",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};

const target_t t_2650a = {
    "2650A", CPU_A,
    "Signetics SCN2650A",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};

const target_t t_2650b = {
    "2650B", CPU_B,
    "Signetics SCN2650B",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};
