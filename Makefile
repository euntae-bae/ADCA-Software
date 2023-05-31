#Created by Sathya Narayanan N
# Email id: sathya281@gmail.com

#   Copyright (C) 2019  IIT Madras. All rights reserved.

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

SHELL := /bin/bash # Use bash syntax
DC ?=
PROGRAM ?=
#default target board
TARGET ?= artix7_35t
DEBUG ?=
MARCH ?= rv32imac
MABI ?= ilp32
XLEN ?= 32
FLAGS ?=

OBJS = md5.o murmur.o jhash.o #sw.o hw.o
#HWOBJS = md5.o murmur.o jhash.o hw.o
cleanOBJS = md5.o murmur.o jhash.o sw.o hw.o

OBJS_TEMP = ./output/md5.o ./output/murmur.o ./output/jhash.o

md5.o: md5.h md5.c
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./md5.c -o ./output/md5.o -lm -lgcc

murmur.o: murmur.h murmur.c
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./murmur.c -o ./output/murmur.o -lm -lgcc

jhash.o: jhash.h jhash.c
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./jhash.c -o ./output/jhash.o -lm -lgcc

sw.o: sw.c md5.h murmur.h jhash.h
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./sw.c -o ./output/sw.o -lm -lgcc

hw.o: hw.c
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./hw.c -o ./output/hw.o -lm -lgcc

all: create_dir
	make $(PROGRAM).riscv

$(PROGRAM).riscv: create_dir $(OBJS) sw.o hw.o
#	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -w $(DC) -mcmodel=medany -static -std=gnu99 -fno-builtin-printf $(FLAGS)-I$(bsplib) -I$(bspinc) -I$(bspdri)/plic -I$(bspboard) -c ./$(PROGRAM).c -o ./output/$(PROGRAM).o -lm -lgcc 
#	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -T  $(bspboard)/link.ld $(GENLIB)/gen_lib/libshakti$(XLEN).a ./output/$(PROGRAM).o -o ./output/$(PROGRAM).elf -static -nostartfiles -lm -lgcc
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -T  $(bspboard)/link.ld $(GENLIB)/gen_lib/libshakti$(XLEN).a $(OBJS_TEMP) ./output/sw.o -o ./output/riscv-hash-sw.elf -static -nostartfiles -lm -lgcc
	@riscv$(XLEN)-unknown-elf-gcc -march=$(MARCH) -mabi=$(MABI) -T  $(bspboard)/link.ld $(GENLIB)/gen_lib/libshakti$(XLEN).a ./output/hw.o -o ./output/riscv-hash-hw.elf -static -nostartfiles -lm -lgcc
	@riscv$(XLEN)-unknown-elf-objdump -D ./output/riscv-hash-sw.elf > ./output/riscv-hash-sw.dump
	@riscv$(XLEN)-unknown-elf-objdump -D ./output/riscv-hash-hw.elf > ./output/riscv-hash-hw.dump

create_dir:
	@mkdir -p ./output

