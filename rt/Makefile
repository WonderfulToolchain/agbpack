export WONDERFUL_TOOLCHAIN ?= /opt/wonderful
WF_TARGET ?= gba/rom
include $(WONDERFUL_TOOLCHAIN)/target/gba/makedefs-common.mk

NNPACK_LZSS ?= $(WONDERFUL_TOOLCHAIN)/bin/wf-nnpack-lzss

OBJDIR := build
OUTDIR := out
SRCDIR := src
MKDIRS := $(OBJDIR) $(OUTDIR)
SRCFILES := src/apack.s src/stage1.S src/stage2.S

CFLAGS := -mcpu=arm7tdmi -mtune=arm7tdmi -ffreestanding
ASFLAGS := -x assembler-with-cpp
LDFLAGS :=

DESTDIR ?= $(WF)

vpath %.c $(SRCDIR)
vpath %.s $(SRCDIR)
vpath %.S $(SRCDIR)

.PHONY: all clean install

all: \
	$(OUTDIR)/bootstrap_rom.c \
	$(OUTDIR)/bootstrap_rom_nopack.c \
	$(OUTDIR)/bootstrap_multiboot.c \
	$(OUTDIR)/bootstrap_multiboot_nopack.c

$(OUTDIR)/bootstrap_rom.c: $(SRCFILES) $(OBJDIR)
	$(CC) $(ASFLAGS) -DAPLIB -c -o $(OBJDIR)/stage2_rom.o $(SRCDIR)/stage2.S
	$(OBJCOPY) -O binary $(OBJDIR)/stage2_rom.o $(OBJDIR)/stage2_rom.bin
	$(NNPACK_LZSS) -ewn $(OBJDIR)/stage2_rom.bin $(OBJDIR)/stage2_rom.lzss
	$(CC) $(ASFLAGS) -DAPLIB -c -o $(OBJDIR)/bootstrap_rom.o $(SRCDIR)/stage1.S
	$(OBJCOPY) -O binary $(OBJDIR)/bootstrap_rom.o $(OBJDIR)/bootstrap_rom.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/bootstrap_rom.bin

$(OUTDIR)/bootstrap_rom_nopack.c: $(SRCFILES) $(OBJDIR)
	$(CC) $(ASFLAGS) -c -o $(OBJDIR)/stage2_rom_nopack.o $(SRCDIR)/stage2.S
	$(OBJCOPY) -O binary $(OBJDIR)/stage2_rom_nopack.o $(OBJDIR)/stage2_rom_nopack.bin
	$(NNPACK_LZSS) -ewn $(OBJDIR)/stage2_rom_nopack.bin $(OBJDIR)/stage2_rom_nopack.lzss
	$(CC) $(ASFLAGS) -c -o $(OBJDIR)/bootstrap_rom_nopack.o $(SRCDIR)/stage1.S
	$(OBJCOPY) -O binary $(OBJDIR)/bootstrap_rom_nopack.o $(OBJDIR)/bootstrap_rom_nopack.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/bootstrap_rom_nopack.bin

$(OUTDIR)/bootstrap_multiboot.c: $(SRCFILES) $(OBJDIR)
	$(CC) $(ASFLAGS) -DMULTIBOOT -DAPLIB -c -o $(OBJDIR)/stage2_multiboot.o $(SRCDIR)/stage2.S
	$(OBJCOPY) -O binary $(OBJDIR)/stage2_multiboot.o $(OBJDIR)/stage2_multiboot.bin
	$(NNPACK_LZSS) -ewn $(OBJDIR)/stage2_multiboot.bin $(OBJDIR)/stage2_multiboot.lzss
	$(CC) $(ASFLAGS) -DMULTIBOOT -DAPLIB -c -o $(OBJDIR)/bootstrap_multiboot.o $(SRCDIR)/stage1.S
	$(OBJCOPY) -O binary $(OBJDIR)/bootstrap_multiboot.o $(OBJDIR)/bootstrap_multiboot.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/bootstrap_multiboot.bin

$(OUTDIR)/bootstrap_multiboot_nopack.c: $(SRCFILES) $(OBJDIR)
	$(CC) $(ASFLAGS) -DMULTIBOOT -c -o $(OBJDIR)/stage2_multiboot_nopack.o $(SRCDIR)/stage2.S
	$(OBJCOPY) -O binary $(OBJDIR)/stage2_multiboot_nopack.o $(OBJDIR)/stage2_multiboot_nopack.bin
	$(NNPACK_LZSS) -ewn $(OBJDIR)/stage2_multiboot_nopack.bin $(OBJDIR)/stage2_multiboot_nopack.lzss
	$(CC) $(ASFLAGS) -DMULTIBOOT -c -o $(OBJDIR)/bootstrap_multiboot_nopack.o $(SRCDIR)/stage1.S
	$(OBJCOPY) -O binary $(OBJDIR)/bootstrap_multiboot_nopack.o $(OBJDIR)/bootstrap_multiboot_nopack.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/bootstrap_multiboot_nopack.bin

$(OBJDIR):
	$(info $(shell mkdir -p $(MKDIRS)))

clean:
	rm -r $(OBJDIR) $(OUTDIR)
