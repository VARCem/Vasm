#
# VASM		VARCem Multi-Target Macro Assembler.
#		A simple table-driven assembler for several 8-bit target
#		devices, like the 6502, 6800, 80x, Z80 et al series. The
#		code originated from Bernd B”ckmann's "asm6502" project.
#
#		This file is part of the VARCem Project.
#
#		Makefile for Windows systems using the TCC environment.
#
# Version:	@(#)Makefile.TCC	1.2.1	2023/09/28
#
# Author:	Fred N. van Kempen, <waltje@varcem.com>
#
#		Copyright 2023 Fred N. van Kempen.
#
#		Redistribution and  use  in source  and binary forms, with
#		or  without modification, are permitted  provided that the
#		following conditions are met:
#
#		1. Redistributions of  source  code must retain the entire
#		   above notice, this list of conditions and the following
#		   disclaimer.
#
#		2. Redistributions in binary form must reproduce the above
#		   copyright  notice,  this list  of  conditions  and  the
#		   following disclaimer in  the documentation and/or other
#		   materials provided with the distribution.
#
#		3. Neither the  name of the copyright holder nor the names
#		   of  its  contributors may be used to endorse or promote
#		   products  derived from  this  software without specific
#		   prior written permission.
#
# THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
# "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
# HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
# THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

ifndef ARCH
 ARCH		:= x86
endif
ifndef DEBUG
 DEBUG		:= n
endif


# General options.
DEFS		:= -DALLOW_UNDEFINED_IF


ifndef MOS6502
 MOS6502	:= y
endif
ifeq ($(MOS6502), y)
 DEFS		+= -DUSE_MOS6502
 TARGETS	+= mos6502.o
endif

ifndef MC6800
 MC6800		:= n
endif
ifeq ($(MC6800), y)
 DEFS		+= -DUSE_MC6800
 TARGETS	+= mc6800.o
endif

ifndef MC6805
 MC6805		:= n
endif
ifeq ($(MC6805), y)
 DEFS		+= -DUSE_MC6805
 TARGETS	+= mc6805.o
endif

ifndef MC6809
 MC6809		:= n
endif
ifeq ($(MC6809), y)
 DEFS		+= -DUSE_MC6809
 TARGETS	+= mc6809.o
endif

ifndef MC6811
 MC6811		:= n
endif
ifeq ($(MC6811), y)
 DEFS		+= -DUSE_MC6811
 TARGETS	+= mc6811.o
endif

ifndef CDP1802
 CDP1802	:= n
endif
ifeq ($(CDP1802), y)
 DEFS		+= -DUSE_CDP1802
 TARGETS	+= cdp1802.o
endif

ifndef SCMP
 SCMP		:= y
endif
ifeq ($(SCMP), y)
 DEFS		+= -DUSE_SCMP
 TARGETS	+= ins8060.o
endif

ifndef SCN2650
 SCN2650	:= y
endif
ifeq ($(SCN2650), y)
 DEFS		+= -DUSE_SCN2650
 TARGETS	+= scn2650.o
endif


#########################################################################
#		Nothing should need changing from here on..		#
#########################################################################

# Definitions for this enivonment.
DEVENV		:= tcc
DEPS		= -MMD -MF $*.d
DEPFILE		:= .depends-$(DEVENV)


# Compilation for X86.
ifeq ($(ARCH), x86)
 TARGET		:= 32
 PREFIX		:=
 RCTARGET	:= pe-i386

 AFLAGS		:= -msse2 -mfpmath=sse
 ifeq ($(OPTIM), y)
  DFLAGS	:= -march=native
 else
  DFLAGS	:= -march=i686
 endif
endif

# Compilation for X64.
ifeq ($(ARCH), x64)
 TARGET		:= 64 -D__GNUC__
 PREFIX		:=
 RCTARGET	:= pe-x86-64

 ifeq ($(OPTIM), y)
  DFLAGS	:= -march=native
 endif
endif

# Compilation for ARM.
ifeq ($(ARCH), armv7)
 error Target ARM7 not supported for Windows !
endif

# Compilation for ARM64.
ifeq ($(ARCH), aarch64)
 error Target ARM64 not supported for Windows !
endif


# The various toolchain programs.
CC		:= tcc -m$(TARGET)
CXX		:= tcc -m$(TARGET)
CPP		:= cpp -P
LINK		:= tcc -m$(TARGET)
AR		:= tcc-ar
RANLIB		:= echo
STRIP		:= echo
ifndef CAT
 CAT		:= cat
endif

# Set up the correct toolchain flags.
OPTS		:= -DARCH=$(ARCH)
AOPTS		:= r
LOPTS		:=

# General build options.
ifeq ($(DEBUG), y)
 DFLAGS		+= -ggdb -D_DEBUG
 RCOPTS		:= -D_DEBUG $(RCOPTS)
 AOPTIM		:=
 ifndef COPTIM
  COPTIM	:= -Og
 endif
else
 ifeq ($(OPTIM), y)
  AOPTIM	:= -mtune=native
  ifndef COPTIM
   COPTIM	:= -O3
  endif
 else
  ifndef COPTIM
   COPTIM	:= -O3
  endif
 endif
endif
ifeq ($(PROFILER), y)
 LOPTS		+= -Xlinker -Map=$(PROG).map
endif


# Final versions of the toolchain flags.
CFLAGS		= $(OPTS) $(DFLAGS) $(COPTIM) $(AOPTIM) \
		  $(AFLAGS) $(INCS) $(DEFS) \
		  -mstackrealign \
		  -fno-strict-aliasing \
		  -Wall -Wundef
LDFLAGS		:= $(LOPTS) $(DFLAGS) $(LDIR)
ARFLAGS		:= $(AOPTS)


# Build module rules.
.SUFFIXES:	.c .cpp .o .rc .res

ifeq ($(AUTODEP), y)
%.o:		%.c
		@echo Compiling $<
		@$(CC) $(CFLAGS) $(DEPS) -c $<
else
%.o:		%.c
		@echo Compiling $<
		@$(CC) $(CFLAGS) -c $<

%.d:		%.c $(wildcard $*.d)
		@echo Compiling $<
		@$(CC) $(CFLAGS) $(DEPS) -E $< >NUL
endif


#########################################################################
#		Nothing should need changing from here on..		#
#########################################################################

INCS		+= -Iplat/unix
VPATH		:= plat/unix targets .

PROG		:= vasm
SYSOBJ		:=
OBJ		:= $(SYSOBJ) \
		   main.o error.o symbol.o expr.o func.o input.o \
		   macro.o output.o list.o parse.o pseudo.o \
		   target.o \
		    $(TARGETS)


all:		$(PROG)


vasm:		$(SYSOBJ) $(OBJ)
		@echo Linking $@ ..
		@$(LINK) $(LDFLAGS) -o $@ $(OBJ) $(LIBS) $(LDLIBS)
		$(if $(filter $(DEBUG),y),,@$(STRIP) $@)


clean:
		@echo Cleaning objects..
		@-rm -f *.o
		@-rm -f *.d


clobber:	clean
		@echo Removing executables..
		@-rm -f $(PROG)
		@echo Cleaning libraries..
		@-rm -f *.so
		@-rm -f *.a
#		@-rm -f $(DEPFILE)

depclean:
		@-rm -f $(DEPFILE)
		@echo Creating dependencies..
		@echo # Run "make depends" to re-create this file. >$(DEPFILE)

depends:	DEPOBJ=$(OBJ:%.o=%.d)
depends:	depclean $(DEPOBJ)
		@$(CAT) $(DEPOBJ) >>$(DEPFILE)
		@-rm -f $(DEPOBJ)

$(DEPFILE):


# Module dependencies.
ifeq ($(AUTODEP), y)
-include *.d
else
 ifneq ($(wildcard $(DEPFILE)), )
  include $(wildcard $(DEPFILE))
 endif
endif


# End of Makefile.TCC.
