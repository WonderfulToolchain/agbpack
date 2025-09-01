#ifndef ELF_H_
#define ELF_H_

#include <stdint.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ 
#define ELF_MAGIC 0x464c457f
#else
#define ELF_MAGIC 0x7f454c46
#endif

#define ELF_ELFCLASS32 1
#define ELF_ELFDATA2LSB 1
#define ELF_ET_EXEC 2
#define ELF_EM_ARM 40
#define ELF_EV_CURRENT 1

typedef struct __attribute__((packed)) {
    uint32_t i_magic;
    uint8_t i_class;
    uint8_t i_data;
    uint8_t i_version;
    uint8_t i_osabi;
    uint8_t i_abiversion;
    uint8_t i_pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_ehdr_t;

#define ELF_PT_LOAD 1
#define ELF_PT_ARM_EXIDX 0x70000001

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_phdr_t;

#endif /* ELF_H_ */
