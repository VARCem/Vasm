/*
 * VASM		VARCem Multi-Target Assembler.
 *		A simple table-driven assembler for several 8-bit target
 *		devices, like the 6502, 6800, 80x, Z80 et al series. The
 *		code originated from Bernd B”ckmann's "asm6502" project.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the MOS6502 series of device targets.
 *
 *		We keep two copies of the opcode table - one for the original
 *		NMOS version, which was also used by a number of chips done
 *		by CSG (Commodore) for their systems, and one for the CMOS
 *		version produced later. The CMOS version also has variants
 *		from Rockwell and WDC, with even more changes.
 *
 * Version:	@(#)mos6502.c	1.0.7	2023/05/14
 *
 * Authors:	Fred N. van Kempen, <waltje@varcem.com>
 *		Bernd B”ckmann, <https://codeberg.org/boeckmann/asm6502>
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../global.h"
#include "../error.h"
#include "../target.h"


#define AM_NUM          15		// number of addressing modes
# define AM_ACC		0		// A
# define AM_IMP		1		// (none)
# define AM_IMM		2		// #$12
# define AM_REL		3		// LABEL
# define AM_ZP		4		// $12
# define AM_ZPI		5		// ($12) (C02)
# define AM_ZPR		6		// ($12),LABEL (WDC C02)
# define AM_ZPX		7		// $12,X
# define AM_ZPY		8		// $12,Y
# define AM_ABS		9		// $1234
# define AM_ABX		10		// $1234,X
# define AM_ABY		11		// $1234,Y
# define AM_IND		12		// ($1234)
# define AM_INX		13		// ($1234,X)
# define AM_INY		14		// ($1234),Y
#define AM_INV		-1

#define INV		0xff		// FIXME: wont work with C02 etc!
#define AM_VALID(op, am) (op->opc[am] != INV)


typedef enum {
    ERR_AM = ERR_MAXERR,	// "invalid addressing mode"
    ERR_REG,			// "invalid register"
    ERR_ILLAM,			// "malformed addressing mode"
    ERR_INX,			// "malformed indirect X addressing"
    ERR_INY,			// "malformed indirect Y addressing"
    ERR_OPUNDEFT,		// "undefined operand size"
    ERR_RELRNG,			// "relative jump target out of range"

    ERR_MAXTRG			// last local error message
} trg_errors_t;


#define CPU_NMOS_0	0x00		// first NMOS processor, ROR bug
#define CPU_NMOS_1	0x01		// regular NMOS core
#define CPU_CMOS	0x02		// standard CMOS core
#define CPU_RW		0x04		// Rockwell CMOS
#define CPU_WDC		0x08		// WDC CMOS


typedef struct opcode {
    char	mn[4];
    uint8_t	flags;
    uint8_t	opc[AM_NUM];
} opcode_t;


static const opcode_t opc_nmos[] = {
 { "ADC", CPU_NMOS_0,
  {INV ,INV ,0x69,INV ,0x65,INV ,INV ,0x75,INV ,0x6d,0x7d,0x79,INV ,0x61,0x71}},
 { "AND", CPU_NMOS_0,
  {INV ,INV ,0x29,INV ,0x25,INV ,INV ,0x35,INV ,0x2d,0x3d,0x39,INV ,0x21,0x31}},
 { "ASL", CPU_NMOS_0,
  {0x0a,INV ,INV ,INV ,0x06,INV ,INV ,0x16,INV ,0x0e,0x1e,INV ,INV ,INV ,INV }},
 { "BCC", CPU_NMOS_0,
  {INV ,INV ,INV ,0x90,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BCS", CPU_NMOS_0,
  {INV ,INV ,INV ,0xb0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BEQ", CPU_NMOS_0,
  {INV ,INV ,INV ,0xf0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BIT", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0x24,INV ,INV ,INV ,INV ,0x2c,INV ,INV ,INV ,INV ,INV }},
 { "BMI", CPU_NMOS_0,
  {INV ,INV ,INV ,0x30,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BNE", CPU_NMOS_0,
  {INV ,INV ,INV ,0xd0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BPL", CPU_NMOS_0,
  {INV ,INV ,INV ,0x10,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BRK", CPU_NMOS_0,
  {INV ,0x00,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BVC", CPU_NMOS_0,
  {INV ,INV ,INV ,0x50,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BVS", CPU_NMOS_0,
  {INV ,INV ,INV ,0x70,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLC", CPU_NMOS_0,
  {INV ,0x18,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLD", CPU_NMOS_0,
  {INV ,0xd8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLI", CPU_NMOS_0,
  {INV ,0x58,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLV", CPU_NMOS_0,
  {INV ,0xb8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CMP", CPU_NMOS_0,
  {INV ,INV ,0xc9,INV ,0xc5,INV ,INV ,0xd5,INV ,0xcd,0xdd,0xd9,INV ,0xc1,0xd1}},
 { "CPX", CPU_NMOS_0,
  {INV ,INV ,0xe0,INV ,0xe4,INV ,INV ,INV ,INV ,0xec,INV ,INV ,INV ,INV ,INV }},
 { "CPY", CPU_NMOS_0,
  {INV ,INV ,0xc0,INV ,0xc4,INV ,INV ,INV ,INV ,0xcc,INV ,INV ,INV ,INV ,INV }},
 { "DEC", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0xc6,INV ,INV ,0xd6,INV ,0xce,0xde,INV ,INV ,INV ,INV }},
 { "DEX", CPU_NMOS_0,
  {INV ,0xca,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "DEY", CPU_NMOS_0,
  {INV ,0x88,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "EOR", CPU_NMOS_0,
  {INV ,INV ,0x49,INV ,0x45,INV ,INV ,0x55,INV ,0x4d,0x5d,0x59,INV ,0x41,0x51}},
 { "INC", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0xe6,INV ,INV ,0xf6,INV ,0xee,0xfe,INV ,INV ,INV ,INV }},
 { "INX", CPU_NMOS_0,
  {INV ,0xe8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "INY", CPU_NMOS_0,
  {INV ,0xc8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 {"JMP", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,0x4c,INV ,INV ,0x6c,INV ,INV }},
 { "JSR", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,0x20,INV ,INV ,INV ,INV ,INV }},
 { "LDA", CPU_NMOS_0,
  {INV ,INV ,0xa9,INV ,0xa5,INV ,INV ,0xb5,INV ,0xad,0xbd,0xb9,INV ,0xa1,0xb1}},
 { "LDX", CPU_NMOS_0,
  {INV ,INV ,0xa2,INV ,0xa6,INV ,INV ,INV ,0xb6,0xae,INV ,0xbe,INV ,INV ,INV }},
 { "LDY", CPU_NMOS_0,
  {INV ,INV ,0xa0,INV ,0xa4,INV ,INV ,0xb4,INV ,0xac,0xbc,INV ,INV ,INV ,INV }},
 { "LSR", CPU_NMOS_0,
  {0x4a,INV ,INV ,INV ,0x46,INV ,INV ,0x56,INV ,0x4e,0x5e,INV ,INV ,INV ,INV }},
 { "NOP", CPU_NMOS_0,
  {INV ,0xea,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "ORA", CPU_NMOS_0,
  {INV ,INV ,0x09,INV ,0x05,INV ,INV ,0x15,INV ,0x0d,0x1d,0x19,INV ,0x01,0x11}},
 { "PHA", CPU_NMOS_0,
  {INV ,0x48,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PHP", CPU_NMOS_0,
  {INV ,0x08,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLA", CPU_NMOS_0,
  {INV ,0x68,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLP", CPU_NMOS_0,
  {INV ,0x28,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "ROL", CPU_NMOS_0,
  {0x2a,INV ,INV ,INV ,0x26,INV ,INV ,0x36,INV ,0x2e,0x3e,INV ,INV ,INV ,INV }},
 { "ROR", CPU_NMOS_1,
  {0x6a,INV ,INV ,INV ,0x66,INV ,INV ,0x76,INV ,0x6e,0x7e,INV ,INV ,INV ,INV }},
 { "RTI", CPU_NMOS_0,
  {INV ,0x40,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "RTS", CPU_NMOS_0,
  {INV ,0x60,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SBC", CPU_NMOS_0,
  {INV ,INV ,0xe9,INV ,0xe5,INV ,INV ,0xf5,INV ,0xed,0xfd,0xf9,INV ,0xe1,0xf1}},
 { "SEC", CPU_NMOS_0,
  {INV ,0x38,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SED", CPU_NMOS_0,
  {INV ,0xf8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SEI", CPU_NMOS_0,
  {INV ,0x78,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "STA", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0x85,INV ,INV ,0x95,INV ,0x8d,0x9d,0x99,INV ,0x81,0x91}},
 { "STX", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0x86,INV ,INV ,INV ,0x96,0x8e,INV ,INV ,INV ,INV ,INV }},
 { "STY", CPU_NMOS_0,
  {INV ,INV ,INV ,INV ,0x84,INV ,INV ,0x94,INV ,0x8c,INV ,INV ,INV ,INV ,INV }},
 { "TAX", CPU_NMOS_0,
  {INV ,0xaa,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TAY", CPU_NMOS_0,
  {INV ,0xa8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TSX", CPU_NMOS_0,
  {INV ,0xba,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TXA", CPU_NMOS_0,
  {INV ,0x8a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TXS", CPU_NMOS_0,
  {INV ,0x9a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TYA", CPU_NMOS_0,
  {INV ,0x98,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }}
};

static const opcode_t opc_cmos[] = {
 { "ADC", CPU_CMOS,
  {INV ,INV ,0x69,INV ,0x65,0x72,INV ,0x75,INV ,0x6d,0x7d,0x79,INV ,0x61,0x71}},
 { "AND", CPU_CMOS,
  {INV ,INV ,0x29,INV ,0x25,0x32,INV ,0x35,INV ,0x2d,0x3d,0x39,INV ,0x21,0x31}},
 { "ASL", CPU_CMOS,
  {0x0a,INV ,INV ,INV ,0x06,INV ,INV ,0x16,INV ,0x0e,0x1e,INV ,INV ,INV ,INV }},
 { "BBR", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,0x0f,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BBS", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,0x8f,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BCC", CPU_CMOS,
  {INV ,INV ,INV ,0x90,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BCS", CPU_CMOS,
  {INV ,INV ,INV ,0xb0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BEQ", CPU_CMOS,
  {INV ,INV ,INV ,0xf0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BIT", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x24,INV ,INV ,INV ,INV ,0x2c,INV ,INV ,INV ,INV ,INV }},
 { "BMI", CPU_CMOS,
  {INV ,INV ,INV ,0x30,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BNE", CPU_CMOS,
  {INV ,INV ,INV ,0xd0,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BPL", CPU_CMOS,
  {INV ,INV ,INV ,0x10,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BRA", CPU_CMOS,
  {INV ,INV ,INV ,0x80,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BRK", CPU_CMOS,
  {INV ,0x00,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BVC", CPU_CMOS,
  {INV ,INV ,INV ,0x50,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "BVS", CPU_CMOS,
  {INV ,INV ,INV ,0x70,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLC", CPU_CMOS,
  {INV ,0x18,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLD", CPU_CMOS,
  {INV ,0xd8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLI", CPU_CMOS,
  {INV ,0x58,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CLV", CPU_CMOS,
  {INV ,0xb8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "CMP", CPU_CMOS,
  {INV ,INV ,0xc9,INV ,0xc5,0xd2,INV ,0xd5,INV ,0xcd,0xdd,0xd9,INV ,0xc1,0xd1}},
 { "CPX", CPU_CMOS,
  {INV ,INV ,0xe0,INV ,0xe4,INV ,INV ,INV ,INV ,0xec,INV ,INV ,INV ,INV ,INV }},
 { "CPY", CPU_CMOS,
  {INV ,INV ,0xc0,INV ,0xc4,INV ,INV ,INV ,INV ,0xcc,INV ,INV ,INV ,INV ,INV }},
 { "DEC", CPU_CMOS,
  {0x3a,INV ,INV ,INV ,0xc6,INV ,INV ,0xd6,INV ,0xce,0xde,INV ,INV ,INV ,INV }},
 { "DEX", CPU_CMOS,
  {INV ,0xca,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "DEY", CPU_CMOS,
  {INV ,0x88,INV ,INV ,INV ,0x52,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "EOR", CPU_CMOS,
  {INV ,INV ,0x49,INV ,0x45,INV ,INV ,0x55,INV ,0x4d,0x5d,0x59,INV ,0x41,0x51}},
 { "INC", CPU_CMOS,
  {0x1a,INV ,INV ,INV ,0xe6,INV ,INV ,0xf6,INV ,0xee,0xfe,INV ,INV ,INV ,INV }},
 { "INX", CPU_CMOS,
  {INV ,0xe8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "INY", CPU_CMOS,
  {INV ,0xc8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 {"JMP", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,0x4c,0x7c,INV ,0x6c,INV ,INV }},
 { "JSR", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,0x20,INV ,INV ,INV ,INV ,INV }},
 { "LDA", CPU_CMOS,
  {INV ,INV ,0xa9,INV ,0xa5,0xb2,INV ,0xb5,INV ,0xad,0xbd,0xb9,INV ,0xa1,0xb1}},
 { "LDX", CPU_CMOS,
  {INV ,INV ,0xa2,INV ,0xa6,INV ,INV ,INV ,0xb6,0xae,INV ,0xbe,INV ,INV ,INV }},
 { "LDY", CPU_CMOS,
  {INV ,INV ,0xa0,INV ,0xa4,INV ,INV ,0xb4,INV ,0xac,0xbc,INV ,INV ,INV ,INV }},
 { "LSR", CPU_CMOS,
  {0x4a,INV ,INV ,INV ,0x46,INV ,INV ,0x56,INV ,0x4e,0x5e,INV ,INV ,INV ,INV }},
 { "NOP", CPU_CMOS,
  {INV ,0xea,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "ORA", CPU_CMOS,
  {INV ,INV ,0x09,INV ,0x05,0x12,INV ,0x15,INV ,0x0d,0x1d,0x19,INV ,0x01,0x11}},
 { "PHA", CPU_CMOS,
  {INV ,0x48,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PHP", CPU_CMOS,
  {INV ,0x08,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PHX", CPU_CMOS,
  {INV ,0xda,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PHY", CPU_CMOS,
  {INV ,0x5a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLA", CPU_CMOS,
  {INV ,0x68,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLP", CPU_CMOS,
  {INV ,0x28,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLX", CPU_CMOS,
  {INV ,0xfa,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "PLY", CPU_CMOS,
  {INV ,0x7a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "RMB", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,0x07,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "ROL", CPU_CMOS,
  {0x2a,INV ,INV ,INV ,0x26,INV ,INV ,0x36,INV ,0x2e,0x3e,INV ,INV ,INV ,INV }},
 { "ROR", CPU_NMOS_1,
  {0x6a,INV ,INV ,INV ,0x66,INV ,INV ,0x76,INV ,0x6e,0x7e,INV ,INV ,INV ,INV }},
 { "RTI", CPU_CMOS,
  {INV ,0x40,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "RTS", CPU_CMOS,
  {INV ,0x60,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SBC", CPU_CMOS,
  {INV ,INV ,0xe9,INV ,0xe5,0xf2,INV ,0xf5,INV ,0xed,0xfd,0xf9,INV ,0xe1,0xf1}},
 { "SEC", CPU_CMOS,
  {INV ,0x38,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SED", CPU_CMOS,
  {INV ,0xf8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SEI", CPU_CMOS,
  {INV ,0x78,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "SMB", CPU_CMOS,
  {INV ,INV ,INV ,INV ,INV ,INV ,0x87,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "STA", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x85,0x92,INV ,0x95,INV ,0x8d,0x9d,0x99,INV ,0x81,0x91}},
 { "STP", CPU_CMOS|CPU_WDC,
  {INV ,0xdb,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "STX", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x86,INV ,INV ,INV ,0x96,0x8e,INV ,INV ,INV ,INV ,INV }},
 { "STY", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x84,INV ,INV ,0x94,INV ,0x8c,INV ,INV ,INV ,INV ,INV }},
 { "STZ", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x64,INV ,INV ,0x74,INV ,0x9c,0x9e,INV ,INV ,INV ,INV }},
 { "TAX", CPU_CMOS,
  {INV ,0xaa,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TAY", CPU_CMOS,
  {INV ,0xa8,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TRB", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x14,INV ,INV ,INV ,INV ,0x1c,INV ,INV ,INV ,INV ,INV }},
 { "TSB", CPU_CMOS,
  {INV ,INV ,INV ,INV ,0x04,INV ,INV ,INV ,INV ,0x0c,INV ,INV ,INV ,INV ,INV }},
 { "TSX", CPU_CMOS,
  {INV ,0xba,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TXA", CPU_CMOS,
  {INV ,0x8a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TXS", CPU_CMOS,
  {INV ,0x9a,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "TYA", CPU_CMOS,
  {INV ,0x98,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }},
 { "WAI", CPU_CMOS|CPU_WDC,
  {INV ,0xcb,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV ,INV }}
};


static const uint16_t	am_size[AM_NUM] = { 1,1,2,2,2,2,2,2,2,3,3,3,3,2,2 };


static const char *err_msg[ERR_MAXTRG - ERR_MAXERR] = {
   "invalid addressing mode",
   "invalid register",
   "malformed indirect X addressing",
   "malformed indirect Y addressing",
   "malformed addressing mode",
   "undefined operand size",
   "relative jump target out of range"
};


/* Handle the Implied and Accumulator modes. */
static int
op_imp_acc(char **p, int pass, const opcode_t *instr)
{
    char id[ID_LEN];
    int am = AM_IMP;
    char *str = *p;

    /* We only handle Implied and Accumulator. */
    if ((instr->opc[AM_ACC] == INV) &&
 	(instr->opc[AM_IMP] == INV)) return AM_INV;

    /* See if we have a register name. */
    if (! IS_END(**p)) {
	/* Looks like it, get it. */
	upcase(p, id);

	/* We only allow the A register. */
	if (strcmp(id, "A")) {
		*p = str;
#if 0
		/* Not an error, we are TRYING these to AMs.. */
		error(ERR_REG, id);
#else
		return AM_INV;
#endif
	}

	/* OK, all good. */
    }

    if (instr->opc[AM_ACC] != INV)
	am = AM_ACC;
    else if (instr->opc[AM_IMP] != INV)
	am = AM_IMP;
    else
	error(ERR_AM, NULL);

    emit_byte(instr->opc[am], pass);

    return am;
}


/* Handle the Immediate mode. */
static int
op_imm(char **p, int pass, const opcode_t *instr)
{
    int am = AM_IMM;
    value_t v;

    (*p)++;
    if (instr->opc[am] == INV) error(ERR_AM, NULL);
    v = expr(p);
    if (pass == 2)
	if (UNDEFINED(v)) error(ERR_UNDEF, NULL);

    emit_byte(instr->opc[am], pass);
    emit_byte((uint8_t)to_byte(v, 0).v, pass);

    return am;
}


/* Handle the Relative mode. */
static int
op_rel(int pass, const opcode_t *instr, value_t v)
{
    int am = AM_REL;
    uint16_t pct = pc + 2u;
    uint16_t off;

    /* relative branch offsets are in 2-complement */
    /* have to calculate it by hand avoiding implementation defined behaviour */
    /* using unsigned int because int may not be in 2-complement */
    if (pass == 2) {
	if (UNDEFINED(v)) error(ERR_UNDEF, NULL);

	if ((v.v >= pct) && ((uint16_t)(v.v - pct) > 0x7f))
		error(ERR_RELRNG, NULL);
	else if ((pct > v.v) && ((uint16_t)(pct - v.v) > 0x80))
		error(ERR_RELRNG, NULL);
    }

    if (v.v >= pct)
	off = v.v - pct;
    else
	off = (uint16_t)((~0) - (pct - v.v - 1));

    emit_byte(instr->opc[am], pass);
    emit_byte(off & 0xff, pass);

    return am;
}


/* Handle the Indirect mode. */
static int
op_ind(char **p, int pass, const opcode_t *instr)
{
    char id[ID_LEN];
    int am = AM_INV;
    value_t v;

    (*p)++;
    v = expr(p);
    skip_white(p);

    /* indirect X addressing mode? */
    if (**p == ',') {
	skip_curr_and_white(p);
	ident_upcase(p, id);
	if (strcmp(id, "X"))
		error(ERR_INX, NULL);
	am = AM_INX;
	skip_white(p);
	if (**p != ')')
		error(ERR_CLBR, NULL);
	skip_curr_and_white(p);
   } else {
	if (**p != ')')
		error(ERR_CLBR, NULL);
	skip_curr_and_white(p);

	/* indirect Y addressing mode? */
	if (**p == ',') {
		skip_curr_and_white(p);
		ident_upcase(p, id);

		if (strcmp(id, "Y"))
			error(ERR_INY, NULL);
		am = AM_INY;
	} else
		am = AM_IND;
    }

    if ((instr->opc[am]) == INV)
	error(ERR_AM, NULL);

    if (pass == 2) {
	if (UNDEFINED(v))
		error(ERR_UNDEF, NULL);
	if (((am == AM_INX) || am == (AM_INY)) && (TYPE(v) != TYPE_BYTE))
		error(ERR_ILLTYPE, NULL);
    }

    emit_byte(instr->opc[am], pass);
    if (am == AM_IND)
	emit_word(v.v, pass);
    else
	emit_byte((uint8_t)to_byte(v, 0).v, pass);

    return am;
}


/* Handle the absolute X and Y, zeropage X and Y modes. */
static int
op_abxy_zpxy(char **p, int pass, const opcode_t *instr, value_t v)
{
    char id[ID_LEN];
    int am = AM_INV;

    ident_upcase(p, id);

    /* Test for absolute and zeropage X addressing. */
    if (! strcmp(id, "X")) {
	if ((TYPE(v) == TYPE_BYTE) && AM_VALID(instr, AM_ZPX))
		am = AM_ZPX;
	else if (AM_VALID(instr, AM_ABX))
		am = AM_ABX;
	else
		error(ERR_AM, NULL);
    }

    /* Test for absolute and zeropage Y addressing. */
    else if (! strcmp(id, "Y")) {
	if ((TYPE(v) == TYPE_BYTE) && AM_VALID(instr, AM_ZPY))
		am = AM_ZPY;
	else if (AM_VALID(instr, AM_ABY))
		am = AM_ABY;
	else
		error(ERR_AM, NULL);
    }
    else
	error(ERR_AM, NULL);

    if (pass == 2)
	if (UNDEFINED(v)) error(ERR_UNDEF, NULL);

    emit_byte(instr->opc[am], pass);
    if ((am == AM_ZPX) || (am == AM_ZPY))
	emit_byte((uint8_t)to_byte(v, 0).v, pass);
    else
	emit_word(v.v, pass);

    return am;
}


/* Handle the Absolute and Zeropage modes. */
static int
op_abs_zp(int pass, const opcode_t *instr, value_t v)
{
    int am = AM_INV;

    if ((TYPE(v) == TYPE_BYTE) && AM_VALID(instr, AM_ZP)) {
	am = AM_ZP;

	if (pass == 2) {
		if (UNDEFINED(v)) error(ERR_UNDEF, NULL);
	}

	emit_byte(instr->opc[am], pass);
	emit_byte((uint8_t)to_byte(v, 0).v, pass);
    } else if (AM_VALID(instr, AM_ABS)) {
	am = AM_ABS;

	if (pass == 2) {
		if (UNDEFINED(v)) error(ERR_UNDEF, NULL);
	}

	emit_byte(instr->opc[am], pass);
	emit_word(v.v, pass);
    } else
	error(ERR_AM, NULL);

    return am;
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
    int am = AM_INV;
    value_t v;

    /* Convert the mnemonic to uppercase. */
    ident_upcase(p, id);

    /* First get instruction for given mnemonic. */
    op = get_mnemonic((const opcode_t *)trg->priv, trg->priv2, id);
    if (op == NULL)
	error(ERR_INSTR, NULL);

    /* Found, so get addressing mode. */
    skip_white_and_comment(p);

    /* Try Accumulator or implied first. */
    if ((am = op_imp_acc(p, pass, op)) != AM_INV) {
	/* Good, we handled that. */
    } else if (**p == '#') {
	/* Immediate mode. */
	am = op_imm(p, pass, op);
    } else if (**p == '(') {
	/* Indirect addressing modes. */
	am = op_ind(p, pass, op);
    } else {
	/* Relative and absolute addressing modes. */
	v = expr(p);
	skip_white(p);

	/* Relative instruction mode if instruction supports it. */
	if (op->opc[AM_REL] != INV) {
		am = op_rel(pass, op, v);
	} else if (**p == ',') {
		/* .. else we try the possible absolute addressing modes. */
		skip_curr_and_white(p);
		am = op_abxy_zpxy(p, pass, op, v);
	} else {
		/* Must be absolute or zeropage addressing. */
		am = op_abs_zp(pass, op, v);
	}
    }

    /* Bail out if that could not be done. */
    if (am == AM_INV)
	error(ERR_AM, id);

    /* Return the amount of code generated. */
    return am_size[am];
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


const target_t t_6502_old = {
    "6502_old", CPU_CMOS,
    "MOS6502 (old)",
    opc_nmos, (sizeof(opc_nmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};

const target_t t_6502_nmos = {
    "6502", CPU_NMOS_1,
    "MOS6502",
    opc_nmos, (sizeof(opc_nmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};

const target_t t_csg6510 = {
    "6510", CPU_NMOS_1,
    "CSG6510",
    opc_nmos, (sizeof(opc_nmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};

const target_t t_csg8500 = {
    "8500", CPU_NMOS_1,
    "CSG8500",
    opc_nmos, (sizeof(opc_nmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};

const target_t t_r65c02 = {
    "65c02", CPU_CMOS,
    "Rockwell 65C02",
    opc_cmos, (sizeof(opc_cmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};

const target_t t_w65c02 = {
    "w65c02", CPU_CMOS | CPU_WDC,
    "WDC 65C02",
    opc_cmos, (sizeof(opc_cmos) / sizeof(opcode_t)),
    t_error, t_instr, t_instr_ok
};
