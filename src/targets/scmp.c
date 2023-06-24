/*
 * VASM		VARCem Multi-Target Macro Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the National Semiconductor SC/MP (INS80xx) series.
 *
 *		The SC/MP ("scamp") processor had three revisions- the SC/MP,
 *		known as ISP-8A/500D (usually a white ceramic DIP), which was
 *		the initial model with 46 instructions, no stack, no call
 *		mechanism other than using XPPR, and serial support in the
 *		E register.
 *
 *		The followup-model, the ISP-8A/600, was the same chip, but
 *		fabricated using the newer NMOS process instead of Bipolar
 *		as used for the 500D chip, resulting in a slightly faster
 *		chip that only needed a single +5V power supply, and using
 *		less power. Other than instruction timings, everything else
 *		was the same, so for code, nothing changed.
 *
 * Version:	@(#)scmp.c	1.0.1	2023/06/19
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
    GRP_IMP = 0,		// implied
    GRP_IMM,			// $12
    GRP_PTR,			// P1,P2,P3
    GRP_REL,			// relative (disp) or pointer disp(ptr)
    GRP_MEM,			// relative,auto-ptr @disp(ptr)
    GRP_JS,			// JS pseudo
    GRP_ABS			// $1234 (807x models)
} trg_groups_t;

typedef enum {
    ERR_NOTIMP = ERR_MAXERR,	// "instruction not implemented.."
    ERR_AM,			// "invalid addressing mode"
    ERR_PTR,			// "invalid pointer register"
    ERR_RELRNG,			// "relative target out of range"

    ERR_MAXTRG			// last local error message
} trg_errors_t;


#define CPU_0	0x00			// original ISP-8A/500D
#define CPU_A	0x01			// standard ISP-8A/600 (INS8060)
#define CPU_B	0x02			// standard INS807x


typedef struct opcode {
    char	mn[5];			// mnemonic
    uint8_t	cpu;			// CPU support flags
    uint8_t	opcode;			// base opcode
    int8_t	grp;			// instruction group
} opcode_t;


static const opcode_t opcodes[] = {
    { "ADD",	CPU_0,	0xf0,	GRP_MEM		},
    { "ADE",	CPU_0,	0x70,	GRP_IMP		},
    { "ADI",	CPU_0,	0xf4,	GRP_IMM		},
    { "AND",	CPU_0,	0xd0,	GRP_MEM		},
    { "ANE",	CPU_0,	0x50,	GRP_IMP		},
    { "ANI",	CPU_0,	0xd4,	GRP_IMM		},
    { "CAD",	CPU_0,	0xf8,	GRP_MEM		},
    { "CAE",	CPU_0,	0x78,	GRP_IMP		},
    { "CAI",	CPU_0,	0xfc,	GRP_IMM		},
    { "CAS",	CPU_0,	0x07,	GRP_IMP		},
    { "CCL",	CPU_0,	0x02,	GRP_IMP		},
    { "CSA",	CPU_0,	0x06,	GRP_IMP		},
    { "DAD",	CPU_0,	0xe8,	GRP_MEM		},
    { "DAE",	CPU_0,	0x68,	GRP_IMP		},
    { "DAI",	CPU_0,	0xec,	GRP_IMM		},
    { "DINT",	CPU_0,	0x04,	GRP_IMP		},
    { "DIV",	CPU_B,	0x0d,	GRP_IMP		},
    { "DLD",	CPU_0,	0xb8,	GRP_REL		},
    { "DLY",	CPU_0,	0x8f,	GRP_IMM		},
    { "HALT",	CPU_0,	0x00,	GRP_IMP		},
    { "IEN",	CPU_0,	0x05,	GRP_IMP		},
    { "ILD",	CPU_0,	0xa8,	GRP_REL		},
    { "JMP",	CPU_0,	0x90,	GRP_REL		},
    { "JNZ",	CPU_0,	0x9c,	GRP_REL		},
    { "JP",	CPU_0,	0x94,	GRP_REL		},
    { "JS",	CPU_0,	0xff,	GRP_JS		},
    { "JZ",	CPU_0,	0x98,	GRP_REL		},
    { "LD",	CPU_0,	0xc0,	GRP_MEM		},
    { "LDE",	CPU_0,	0x40,	GRP_IMP		},
    { "LDI",	CPU_0,	0xc4,	GRP_IMM		},
    { "MPY",	CPU_B,	0x2c,	GRP_IMP		},
    { "NOP",	CPU_0,	0x08,	GRP_IMP		},
    { "OR",	CPU_0,	0xd8,	GRP_MEM		},
    { "ORE",	CPU_0,	0x58,	GRP_IMP		},
    { "ORI",	CPU_0,	0xdc,	GRP_IMM		},
    { "RR",	CPU_0,	0x1e,	GRP_IMP		},
    { "RRL",	CPU_0,	0x1f,	GRP_IMP		},
    { "SCL",	CPU_0,	0x03,	GRP_IMP		},
    { "SIO",	CPU_0,	0x19,	GRP_IMP		},
    { "SR",	CPU_0,	0x1c,	GRP_IMP		},	// 3C,0C
    { "SRL",	CPU_0,	0x1d,	GRP_IMP		},	// 3D
    { "ST",	CPU_0,	0xc8,	GRP_MEM		},
    { "SUB",	CPU_B,	0xf8,	GRP_MEM		},
    { "XAE",	CPU_0,	0x01,	GRP_IMP		},
    { "XCH",	CPU_B,	0x01,	GRP_IMP		},	// 01,4D,4E,4F
    { "XOR",	CPU_0,	0xe0,	GRP_MEM		},
    { "XPAH",	CPU_0,	0x34,	GRP_PTR		},
    { "XPAL",	CPU_0,	0x30,	GRP_PTR		},
    { "XPPC",	CPU_0,	0x3c,	GRP_PTR		},
    { "XRE",	CPU_0,	0x60,	GRP_IMP		},
    { "XRI",	CPU_0,	0xe4,	GRP_IMM		}
};


static const char *err_msg[ERR_MAXTRG - ERR_MAXERR] = {
    "instruction not implemented",
    "invalid addressing mode",
    "invalid pointer register",
    "relative target out of range"
};


/* Get the target pointer register P0..P3. */
static int
get_ptr(char **p)
{
    char id[ID_LEN];
    int ptr;

    skip_white(p);

    /* We allow the P (or p) prefix. */
    if (**p == 'P' || **p == 'p')
	(*p)++;

    /* Pointers are 0 through 3. */
    if (**p < '0' || **p > '3')
	return -1;

    /* Convert the name to uppercase. */
    nident(p, id);
    ptr = atoi(id);
    if (ptr < 0 || ptr > 3)
	return -1;

    return ptr;
}


static int
get_ea(uint16_t addr, int pass)
{
    uint16_t pct, off;
    int16_t disp;

    /* Sign-extend the (12-bit) target if needed. */
    if (addr & 0x0800)
	addr |= 0xf000;
    disp = (int16_t)addr;

    pct = pc + 1;
    off = disp - pct;

    if (pass == 2) {
	if (disp >= pct) {
		if (off > 0x7f)
			error(ERR_RELRNG, NULL);
	} else {
		if (off < 0x80)
			error(ERR_RELRNG, NULL);
	}
    }

    return off;
}


/*
 * Handle the Implied group.
 *
 * HALT
 */
static int
grp_imp(char **p, int pass, const opcode_t *instr)
{
    emit_byte(instr->opcode, pass);

    return 1;
}


/*
 * Handle the Immediate group.
 *
 * LDI value
 */
static int
grp_imm(char **p, int pass, const opcode_t *instr)
{
    value_t v;
    int x;

    skip_white(p);

    /* Some assemblers allow the '#' character here. */
    if (**p == '#')
	(*p)++;

    /* Get the (immediate) argument. */
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }

    /* Make sure this is valid for a byte. */
    x = v.v;
    if (x > 0xff)
	error(ERR_RNG_BYTE, NULL);
    x &= 0xff;

    emit_byte(instr->opcode, pass);
    emit_byte((uint8_t)x, pass);

    return 2;
}


/*
 * Handle the Pointer group.
 *
 * XPAH  ptr
 */
static int
grp_ptr(char **p, int pass, const opcode_t *instr)
{
    int ptr;

    /* Get the pointer register. */
    ptr = get_ptr(p);
    if (ptr < 0)
	error(ERR_PTR, NULL);

    emit_byte(instr->opcode|ptr, pass);

    return 1;
}


/*
 * Handle the Relative/Pointer mode.
 *
 * ILD	disp
 * DLD	disp(ptr)
 *
 * JZ	disp(ptr)
 * JMP	disp
 */
static int
grp_rel(char **p, int pass, const opcode_t *instr)
{
    value_t v = { 0 };
    uint16_t off;
    int ptr = 0;

    skip_white(p);

    /* Get the (relative) argument. */
    if (**p != '(') {
	/* We have some kind of displacement. */
	v = expr(p);
	if (pass == 2) {
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);
	}
    }

    skip_white(p);

    /* Get the pointer register if we have one. */
    if (**p == '(') {
	(*p)++;
	ptr = get_ptr(p);
	if (ptr < 0)
		error(ERR_PTR, NULL);

	if (**p != ')')
		error(ERR_UNBALANCED, NULL);
	(*p)++;
    }

    if (ptr == 0)
	off = get_ea((uint16_t)v.v, pass);
    else
	off = (uint16_t)v.v;

    /* If not DLD or ILD (so, jumps) .. */
    if (instr->opcode != 0xa8 && instr->opcode != 0xb8)
	off--;

    emit_byte(instr->opcode|ptr, pass);
    emit_byte(off & 0xff, pass);

    return 2;
}


/*
 * Handle the Memory mode.
 *
 * LD	disp
 * ST	disp(ptr)
 * AND	@disp(ptr)
 * OR	@-disp(ptr)
 */
static int
grp_mem(char **p, int pass, const opcode_t *instr)
{
    value_t v = { 0 };
    uint16_t off;
    uint8_t op;
    int ind = 0, ptr = 0;

    skip_white(p);
    if (**p == '@') {
	(*p)++;
	ind = 1;
    }

    /* Get the (relative) argument. */
    if (**p != '(') {
	/* We have some kind of displacement. */
	v = expr(p);
	if (pass == 2) {
		if (UNDEFINED(v))
			error(ERR_UNDEF, NULL);
	}
    }

    skip_white(p);

    /* Get the pointer register if we have one. */
    if (**p == '(') {
	(*p)++;
	ptr = get_ptr(p);
	if (ptr < 0)
		error(ERR_PTR, NULL);

	if (**p != ')')
		error(ERR_UNBALANCED, NULL);
	(*p)++;
    }

    /* Auto mode requires P1..P3. */
    if (ind) {
	if (ptr == 0)
		error(ERR_PTR, NULL);
    }

    if (ptr == 0)
	off = get_ea((uint16_t)v.v, pass);
    else
	off = (uint16_t)v.v;

    op = instr->opcode;
    if (ind)
	op |= 0x04;
    op |= ptr;

    emit_byte(op, pass);
    emit_byte(off & 0xff, pass);

    return 2;
}


/*
 * Handle the JS pseudo.
 *
 *   JS  ptr,expr
 *
 * Generated code:
 *
 *  C4 hh   LDI  >(expr-1)
 *  34p     XPAH ptr
 *  C4 ll   LDI  <(expr-1)
 *  30p     XPAL ptr
 *  3Cp     XPPC ptr
 *
 * This was originally a macro in the NSC cross assembler.
 */
static int
grp_js(char **p, int pass, const opcode_t *instr)
{
    value_t v;
    int ptr;

    /* Get the pointer register. */
    ptr = get_ptr(p);
    if (ptr < 0)
	error(ERR_PTR, NULL);

    /* Skip the comma. */
    if (**p != ',')
	error(ERR_COMMA, NULL);
    (*p)++;

    /* Grab the expression. */
    skip_white(p);
    v = expr(p);
    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
    }

    /* Thank you, NSC. */
    v.v--;
    v.v &= 0xffff;

    /* Generate the code for this. */
    emit_byte(0xc4, pass);		// LDI >(expr-1)
    emit_byte((v.v >> 8), pass);
    emit_byte(0x34 + ptr, pass);	// XPAH ptr
    emit_byte(0xc4, pass);		// LDI <(expr-1)
    emit_byte((v.v & 0xff), pass);
    emit_byte(0x30 + ptr, pass);	// XPAL ptr
    emit_byte(0x3c + ptr, pass);	// XPPC ptr

    return 7;
}


#if 0
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
#endif


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
	error(ERR_INSTR, id);

    /* Make sure we are allowed to use this instruction. */
    if (op->cpu > trg->flags)
	error(ERR_NOTIMP, id);

    /* Found, so get addressing mode. */
    skip_white(p);

    switch (op->grp) {
	case GRP_IMP:
		bytes = grp_imp(p, pass, op);
		break;

	case GRP_IMM:
		bytes = grp_imm(p, pass, op);
		break;

	case GRP_PTR:
		bytes = grp_ptr(p, pass, op);
		break;

	case GRP_REL:
		bytes = grp_rel(p, pass, op);
		break;

	case GRP_MEM:
		bytes = grp_mem(p, pass, op);
		break;

#if 0
	case AM_ABS:
		bytes = op_abs(p, pass, op);
		break;
#endif

	case GRP_JS:
		bytes = grp_js(p, pass, op);
		break;

	default:
		emit_byte(op->opcode, pass);
		bytes = 1;
		break;
    }

    /* Bail out if that could not be done. */
    if (bytes == 0)
	error(ERR_AM, id);

    while (! IS_END(**p))
	(*p)++;

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


const target_t t_scmp = {
    "SCMP", CPU_0,
    "NS SC/MP (ISP-8A/500D)",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};

const target_t t_ins8060 = {
    "INS8060", CPU_A,
    "NS SC/MP-II (INS8060, ISP-8A/600)",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};

const target_t t_ins8070 = {
    "INS8070", CPU_B,
    "NS SC/MP-III (INS807x)",
    opcodes, sizeof(opcodes)/sizeof(opcode_t),
    t_error, t_instr, t_instr_ok
};
