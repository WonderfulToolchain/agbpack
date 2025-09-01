export WONDERFUL_TOOLCHAIN ?= /opt/wonderful
WF_TARGET ?= gba/rom
include $(WONDERFUL_TOOLCHAIN)/target/gba/makedefs-common.mk

OBJDIR := build
OUTDIR := out
SRCDIRS := src
MKDIRS := $(OBJDIR) $(OUTDIR)

CFLAGS := -mcpu=arm7tdmi -mtune=arm7tdmi -ffreestanding
ASFLAGS := -x assembler-with-cpp
LDFLAGS :=

DESTDIR ?= $(WF)

vpath %.c $(SRCDIRS)
vpath %.s $(SRCDIRS)
vpath %.S $(SRCDIRS)

.PHONY: all clean install

all: $(OUTDIR)/crt0_rom.c $(OUTDIR)/crt0_multiboot.c

$(OUTDIR)/crt0_rom.c: crt0.S $(OBJDIR)
	$(CC) $(ASFLAGS) -DAPLIB -c -o $(OBJDIR)/crt0_rom.o $<
	$(OBJCOPY) -O binary $(OBJDIR)/crt0_rom.o $(OBJDIR)/crt0_rom.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/crt0_rom.bin

$(OUTDIR)/crt0_multiboot.c: crt0.S $(OBJDIR)
	$(CC) $(ASFLAGS) -DAPLIB -DMULTIBOOT -c -o $(OBJDIR)/crt0_multiboot.o $<
	$(OBJCOPY) -O binary $(OBJDIR)/crt0_multiboot.o $(OBJDIR)/crt0_multiboot.bin
	wf-bin2c $(OUTDIR) $(OBJDIR)/crt0_multiboot.bin

$(OBJDIR):
	$(info $(shell mkdir -p $(MKDIRS)))

clean:
	rm -r $(OBJDIR) $(OUTDIR)
