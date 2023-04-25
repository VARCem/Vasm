#!/bin/sh
vasm -p6502 -s -l foo.lst -o foo.hex '-DLOAD=$400' foo.asm
