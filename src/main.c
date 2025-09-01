#include "elf.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Big endian targets not supported!
#endif

#include "libapultra.h"
#include "crt0_multiboot_bin.h"
#include "crt0_rom_bin.h"

#define VERSION "0.3.0"

#define AGB_EWRAM_START 0x02000000
#define AGB_EWRAM_END   0x0203FFFF
#define AGB_EWRAM_SIZE  0x40000
#define AGB_IWRAM_START 0x03000000
#define AGB_IWRAM_END   0x03007FFF
#define AGB_IWRAM_SIZE  0x8000
#define AGB_ROM_START   0x08000000
#define AGB_ROM_END     0x09FFFFFF
#define AGB_ROM_SIZE    0x2000000
#define MAX(a,b) (((a) < (b)) ? (b) : (a))

static bool phdr_supports_type(uint32_t type) {
    return type == ELF_PT_LOAD || type == ELF_PT_ARM_EXIDX;
}

static inline bool address_is_ewram(uint32_t address) {
    return address >= AGB_EWRAM_START && address <= AGB_EWRAM_END;
}

static inline bool address_is_iwram(uint32_t address) {
    return address >= AGB_IWRAM_START && address <= AGB_IWRAM_END;
}

static bool address_supports_8bit_writes(uint32_t address) {
    return address_is_ewram(address) || address_is_iwram(address);
}

bool verbose = false;
const char *cue_lzss_path = NULL;

static void *checked_malloc(size_t size) {
    void *buffer = malloc(size);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    return buffer;
}

static void checked_fwrite(const void *ptr, size_t size, FILE *fp) {
    if (fwrite(ptr, size, 1, fp) < 1) {
        fprintf(stderr, "Could not write to file!\n");
        exit(1);
    }
}

static void *read_file(const char *filename, int *length) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open \"%s\"!\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (length != NULL) {
        *length = size;
    }

    if (size <= 0) {
        fprintf(stderr, "Could not open \"%s\"! (empty file?)\n", filename);
        fclose(fp);
        exit(1);
    }

    void *buffer = checked_malloc(size);
    if (fread(buffer, size, 1, fp) < 1) {
        fprintf(stderr, "Could not read \"%s\"!\n", filename);
        free(buffer);
        fclose(fp);
        exit(1);
    }

    fclose(fp);
    return buffer;
}

static void write_file(const char *filename, const void *buffer, size_t length) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open \"%s\"!\n", filename);
        exit(1);
    }

    checked_fwrite(buffer, length, fp);
    fclose(fp);
}

static void print_help(int argc, char **argv) {
    printf("Usage: %s [-0hv] [-L <path>] <input> <output>\n\n", argc && argv[0] ? argv[0] : "agbpack");
    printf("  -0         Disable compression.\n");
    printf("  -L <path>  Use LZSS compression for VRAM data via external nnpack-lzss.\n");
    printf("  -h         Print help information.\n");
    printf("  -V         Print version information.\n");
    printf("  -v         Enable verbose logging.\n");
}

static void print_version(void) {
    printf("agbpack %s\n", VERSION);
}

typedef struct __attribute__((packed)) {
    uint32_t source, dest, flags;
} section_entry_t;
_Static_assert(sizeof(section_entry_t) == 12, "Invalid section_entry_t size!");

typedef struct {
    const void *source;
    uint32_t offset;
    uint32_t length;
    bool managed;
    uint32_t reserve_at_end;
} copy_entry_t;

#define ELF_PT_PROCESSED 0x6ffffff0
#define MAX_ENTRIES 1024

typedef struct {
    section_entry_t section_entries[MAX_ENTRIES];
    copy_entry_t copy_entries[MAX_ENTRIES];
    int entries_count;
} pack_state_t;
                
static void checked_increment_entries_count(pack_state_t *state) {
    state->entries_count++;
    if (state->entries_count >= MAX_ENTRIES) {
        fprintf(stderr, "Too many sections!\n");
        exit(1);
    }
}


#define BIOS_MODE_COPY 0
#define BIOS_MODE_FILL (1 << 24)
#define BIOS_UNIT_HALFWORDS 0
#define BIOS_UNIT_WORDS (1 << 26)
// IME is guaranteed to be zero
#define ZERO_FILL_ADDRESS 0x04000208

static void append_bios_copy_section(pack_state_t *state, const void *source, uint32_t destination, uint32_t length, bool fill) {
    section_entry_t *entry = &state->section_entries[state->entries_count];
    uint32_t orig_length = length;
    if (!(length & 3)) {
        length >>= 2;
        entry->flags = BIOS_UNIT_WORDS;
    } else if (!(length & 1)) {
        length >>= 1;
        entry->flags = BIOS_UNIT_HALFWORDS;
    } else {
        fprintf(stderr, "Fill area not aligned: %d @ %08X\n", length, destination);
        exit(1);
    }
    if (length >= (1 << 21)) {
        fprintf(stderr, "Fill area too large: %d @ %08X\n", length, destination);
        exit(1);
    }
    entry->flags |= length;
    entry->flags |= (fill ? BIOS_MODE_FILL : BIOS_MODE_COPY);
    entry->source = fill ? ZERO_FILL_ADDRESS : 0;
    entry->dest = destination;

    if (!fill) {
        state->copy_entries[state->entries_count].source = source;
        state->copy_entries[state->entries_count].length = orig_length;
    }

    checked_increment_entries_count(state);
}

// Decompress data directly
#define COMPRESS_MODE_NORMAL 1
// Copy data to end of EWRAM, then decompress in EWRAM
#define COMPRESS_MODE_EWRAM_FINAL 2
// Decompress data to end of EWRAM, then BIOS copy to VRAM
#define COMPRESS_MODE_VRAM_COPY 3

static void append_try_compress_section(pack_state_t *state, const void *source, uint32_t destination, uint32_t length, uint32_t window_size, int compress_mode) {
    if (compress_mode) {
        void *packed;
        int result;
        if (compress_mode == COMPRESS_MODE_VRAM_COPY && cue_lzss_path != NULL) {
            char tmp_in[256+1];
            char tmp_out[256+1];
            char command[4096+1];

            command[4096] = 0;
            sprintf(tmp_in, ".agbpack.i%d.%d.bin", getpid(), rand() & 32767);
            sprintf(tmp_out, ".agbpack.o%d.%d.bin", getpid(), rand() & 32767);
            snprintf(command, 4096, "\"%s\" -evo %s %s", cue_lzss_path, tmp_in, tmp_out);
            write_file(tmp_in, source, length);
            if (system(command) < 0) {
                fprintf(stderr, "Error running \"%s\"\n", cue_lzss_path);
                exit(1);
            }
            packed = read_file(tmp_out, &result);
        } else {
            size_t packed_buffer_size = apultra_get_max_compressed_size(length);
            packed = checked_malloc(packed_buffer_size);
            result = apultra_compress(source, packed, length, packed_buffer_size, 0, window_size, 0, NULL, NULL);
        }
        if (result >= 0 && result < length) {
            if (result > 0 && verbose) printf("-> Compressed %d -> %d bytes\n", length, result);
            if (compress_mode == COMPRESS_MODE_VRAM_COPY && cue_lzss_path == NULL) {
                if (!(length & 3)) {
                    fprintf(stderr, "VRAM section not aligned to 4!\n");
                    exit(1);
                }

                uint32_t intermediary_location = AGB_EWRAM_END + 1 - length;

                state->section_entries[state->entries_count].source = 0;
                state->section_entries[state->entries_count].dest = intermediary_location;
                state->section_entries[state->entries_count].flags = result | (1 << 31);
                state->copy_entries[state->entries_count].source = packed;
                state->copy_entries[state->entries_count].length = result;
                state->copy_entries[state->entries_count].managed = true;
                state->copy_entries[state->entries_count].reserve_at_end = length;
                checked_increment_entries_count(state);

                state->section_entries[state->entries_count].source = intermediary_location;
                state->section_entries[state->entries_count].dest = destination;
                state->section_entries[state->entries_count].flags = (length >> 2) | BIOS_MODE_COPY | BIOS_UNIT_WORDS;
                checked_increment_entries_count(state);
            } else {
                state->section_entries[state->entries_count].source = 0;
                state->section_entries[state->entries_count].dest = destination;
                state->section_entries[state->entries_count].flags =
                    compress_mode == COMPRESS_MODE_EWRAM_FINAL ? ((1 << 30) | ((result + 31) & ~31))
                    : (compress_mode == COMPRESS_MODE_VRAM_COPY ? ((1 << 29) | result) : ((1 << 31) | result));
                state->copy_entries[state->entries_count].source = packed;
                state->copy_entries[state->entries_count].length = result;
                state->copy_entries[state->entries_count].managed = true;
                state->copy_entries[state->entries_count].reserve_at_end = compress_mode == COMPRESS_MODE_EWRAM_FINAL ? 32 : 0;
                checked_increment_entries_count(state);
            }
            return;
        } else {
            if (result < 0 && verbose) printf("-> Section compression error (%d)\n", result);
            if (result > 0 && verbose) printf("-> Compressed section larger than uncompressed (%d > %d), ignoring\n", result, length);
            free(packed);
        }
    }

    // If not compressing, or compression failed
    append_bios_copy_section(state, source, destination, length, false);
}

int main(int argc, char **argv) {
    pack_state_t state;
    memset(&state, 0, sizeof(pack_state_t));

    // === Parse arguments ===

    bool compress = true;
    int c;
    while ((c = getopt(argc, argv, "0L:hVv")) != -1) switch (c) {
    case '0':
        compress = false;
        break;
    case 'L':
        cue_lzss_path = optarg;
        break;
    case 'h':
        print_help(argc, argv);
        return 0;
    case 'V':
        print_version();
        return 0;
    case 'v':
        verbose = true;
        break;
    }

    if ((argc - optind) != 2) {
        print_help(argc, argv);
        return 0;
    }

    srand(time(NULL));
    if (verbose) print_version();

    // === Process ELF file ===

    bool is_raw = false;
    bool is_elf = false;
    bool is_multiboot = true;
    int input_length = 0;
    uint8_t *input = read_file(argv[optind], &input_length);
    uint32_t entrypoint;

    elf_ehdr_t *ehdr = (elf_ehdr_t*) input;
    if (input_length >= 0xE0 && input[3] == 0xEA && input[0xB2] == 0x96) {
        // This is probably a .gba file, not a .elf file.
        is_raw = true;

        if (!(input[0xC2] == 0x00 && input[0xC3] == 0xEA)) {
            fprintf(stderr, "Not a valid multiboot image!\n");
            exit(1);
        }
        uint32_t branch = *((uint32_t*) &input[0xC0]);
        entrypoint = AGB_EWRAM_START + 0xC8 + ((branch & 0xFFFFFF) << 2);

        if (input_length > AGB_EWRAM_SIZE) {
            fprintf(stderr, "File too large!\n");
            exit(1);
        }
    } else {
        if (input_length < sizeof(elf_ehdr_t)
            || ehdr->i_magic != ELF_MAGIC
            || ehdr->i_class != ELF_ELFCLASS32
            || ehdr->i_data != ELF_ELFDATA2LSB
            || ehdr->type != ELF_ET_EXEC
            || ehdr->machine != ELF_EM_ARM
            || ehdr->version != ELF_EV_CURRENT)
        {
            fprintf(stderr, "Unsupported file!\n");
            exit(1);
        }
        is_elf = true;
        entrypoint = ehdr->entry;
    }

    // === Build image ===

    FILE *outf = fopen(argv[optind + 1], "wb");
    if (outf == NULL) {
        fprintf(stderr, "Could not open \"%s\"!\n", argv[optind + 1]);
        exit(1);
    }

    // - Write ROM data (if not multiboot)

    if (is_elf) {
        for (int i = 0; i < ehdr->phnum; i++) {
            elf_phdr_t *phdr = (elf_phdr_t*) (input + (ehdr->phoff + i * ehdr->phentsize));
                
            if (phdr->paddr >= AGB_ROM_START && phdr->paddr <= AGB_ROM_END) {
                if (!phdr_supports_type(phdr->type)) {
                    fprintf(stderr, "Program header %d, which is in ROM, has unsupported type!", i);
                    exit(1);
                }

                is_multiboot = false;

                if (phdr->filesz) {
                    fseek(outf, phdr->paddr - AGB_ROM_START, SEEK_SET);
                    checked_fwrite(input + phdr->offset, phdr->filesz, outf);
                }

                phdr->type = ELF_PT_PROCESSED;
            }
        }
    }

    if (verbose) printf("Loaded %s %s image\n", is_raw ? ".gba" : ".elf", is_multiboot ? "multiboot" : "cartridge");

    // - Write loader

    fseek(outf, 0, SEEK_END);
    uint32_t rom_loader_offset = ftell(outf);

    const void *crt0_data = is_multiboot ? crt0_multiboot : crt0_rom;
    size_t crt0_size = is_multiboot ? crt0_multiboot_size : crt0_rom_size;
    checked_fwrite(crt0_data, crt0_size, outf);

    // - Copy logo/header data
    
    if (is_raw) {
        // Copy logo/header data from old to new .gba
        fseek(outf, 4, SEEK_SET);
        checked_fwrite(input + 4, 0xC0 - 4, outf);
        fseek(outf, 0, SEEK_END);
    }

    // - Write data streams

    if (is_raw) {
        // Write just one area.
        uint32_t ewram_offset = 0xC8;
        uint32_t ewram_window_bytes = AGB_EWRAM_SIZE - input_length - 32;

        if (verbose) printf("Compressing EWRAM data (%08X - %08X), window = %d bytes\n", AGB_EWRAM_START + ewram_offset, AGB_EWRAM_START + input_length, ewram_window_bytes);
        append_try_compress_section(&state, input + ewram_offset, AGB_EWRAM_START + ewram_offset, input_length - ewram_offset, ewram_window_bytes, COMPRESS_MODE_EWRAM_FINAL);
    }

    if (is_elf) {
        // First, write areas which don't support 8-bit writes.
        for (int i = 0; i < ehdr->phnum; i++) {
            elf_phdr_t *phdr = (elf_phdr_t*) (input + (ehdr->phoff + i * ehdr->phentsize));
            if (phdr->type == ELF_PT_PROCESSED) continue;
            if (!phdr_supports_type(phdr->type)) continue;

            if (!phdr->memsz) {
                if (verbose) printf("Skipping program header %d (empty)\n", i);
                phdr->type = ELF_PT_PROCESSED;
            }
            if (phdr->filesz > phdr->memsz) {
                fprintf(stderr, "Program header %d not supported - filesz > memsz > 0", i);
                exit(1);
            }

            if (phdr->filesz && !address_supports_8bit_writes(phdr->paddr)) {
                if (verbose) printf("Processing program header %d (data)\n", i);
                append_try_compress_section(&state, input + phdr->offset, phdr->paddr, phdr->filesz, 0, compress ? COMPRESS_MODE_VRAM_COPY : 0);
                phdr->type = ELF_PT_PROCESSED;
            }
        }

        // Next, copy/fill non-EWRAM areas.
        // Also collect all EWRAM data into one big section.
        uint8_t ewram_data[AGB_EWRAM_SIZE];
        uint32_t ewram_data_start = AGB_EWRAM_END + 1;
        uint32_t ewram_data_end = AGB_EWRAM_START - 1;
        memset(ewram_data, 0, sizeof(ewram_data));

        for (int i = 0; i < ehdr->phnum; i++) {
            elf_phdr_t *phdr = (elf_phdr_t*) (input + (ehdr->phoff + i * ehdr->phentsize));
            if (phdr->type == ELF_PT_PROCESSED) continue;
            if (!phdr_supports_type(phdr->type)) continue;

            if (is_multiboot && address_is_ewram(phdr->paddr)) { 
                if (phdr->filesz) {
                    if (verbose) printf("Appending program header %d to EWRAM data\n", i);
                    memcpy(ewram_data + phdr->paddr - AGB_EWRAM_START, input + phdr->offset, phdr->filesz);
                    if (ewram_data_start > phdr->paddr) ewram_data_start = phdr->paddr;
                    if (ewram_data_end < (phdr->paddr + phdr->filesz - 1)) ewram_data_end = phdr->paddr + phdr->filesz - 1;
                    phdr->type = ELF_PT_PROCESSED;
                }
                continue;
            }
            if (verbose) printf("Processing program header %d (data)\n", i);
            if (phdr->filesz) {
                append_try_compress_section(&state, input + phdr->offset, phdr->paddr, phdr->filesz, 0, compress ? COMPRESS_MODE_NORMAL : 0);
            } else {
                append_bios_copy_section(&state, NULL, phdr->paddr, phdr->memsz, true);
            }
            phdr->type = ELF_PT_PROCESSED;
        }

        uint32_t ewram_window_bytes = AGB_EWRAM_END + 1 - ewram_data_end - 32;

        // Next, copy EWRAM data.
        if (ewram_data_start <= AGB_EWRAM_END) {
            if (verbose) printf("Compressing EWRAM data (%08X - %08X), window = %d bytes\n", ewram_data_start, ewram_data_end, ewram_window_bytes);
            append_try_compress_section(&state, ewram_data + ewram_data_start - AGB_EWRAM_START, ewram_data_start, ewram_data_end + 1 - ewram_data_start, ewram_window_bytes, compress ? COMPRESS_MODE_EWRAM_FINAL : 0);
        }

        // Next, fill EWRAM areas.
        for (int i = 0; i < ehdr->phnum; i++) {
            elf_phdr_t *phdr = (elf_phdr_t*) (input + (ehdr->phoff + i * ehdr->phentsize));
            if (phdr->type == ELF_PT_PROCESSED) continue;
            if (!phdr_supports_type(phdr->type)) continue;

            if (address_is_ewram(phdr->paddr) && !phdr->filesz) {
                if (verbose) printf("Processing program header %d (bss)\n", i);
                append_bios_copy_section(&state, NULL, phdr->paddr, phdr->memsz, true);
                phdr->type = ELF_PT_PROCESSED;
            } else {
                fprintf(stderr, "Unprocessed program header %d!\n", i);
                exit(1);
            }
        }
    }

    // Finally, add a branch instruction.
    state.section_entries[state.entries_count].source = 0;
    state.section_entries[state.entries_count].dest = entrypoint;
    state.section_entries[state.entries_count].flags = -(((state.entries_count + 1) * sizeof(section_entry_t)) + 4);
    checked_increment_entries_count(&state);

    // Prepare data for the appended header.
    uint32_t copy_offset = (is_multiboot ? AGB_EWRAM_START : AGB_ROM_START) + ftell(outf) + 4;
    uint32_t rom_data_length = 0;
    for (int i = 0; i < state.entries_count; i++) {
        if (state.copy_entries[i].source) {
            state.section_entries[i].source = copy_offset + rom_data_length + state.copy_entries[i].offset;
            rom_data_length += (state.copy_entries[i].length + 3) & ~3;
        }
    }
    checked_fwrite(&rom_data_length, 4, outf);
    for (int i = 0; i < state.entries_count; i++) {
        if (state.copy_entries[i].source) {
            checked_fwrite(state.copy_entries[i].source, state.copy_entries[i].length, outf);

            int remainder = ((state.copy_entries[i].length + 3) & ~3) - state.copy_entries[i].length;
            while (remainder--) fputc(0, outf);

            if (state.copy_entries[i].managed) {
                free((void*) state.copy_entries[i].source);
            }
        }
    }

    uint32_t command_stream_length = state.entries_count * 3;
    checked_fwrite(&command_stream_length, 4, outf);
    checked_fwrite(state.section_entries, state.entries_count * sizeof(section_entry_t), outf);

    uint32_t bytes_at_end = AGB_EWRAM_SIZE - ftell(outf);
    for (int i = 0; i < state.entries_count; i++) {
        if (state.copy_entries[i].reserve_at_end && state.copy_entries[i].reserve_at_end > bytes_at_end) {
            fprintf(stderr, "Insufficient bytes at end: %d > %d", state.copy_entries[i].reserve_at_end,  bytes_at_end);
            exit(1);
        }
    }

    // Patch entrypoint for ROM image.
    if (!is_multiboot) {
        fseek(outf, 0, SEEK_SET);
        uint32_t branch = 0xEA000000 | ((rom_loader_offset - 8) >> 2);
        checked_fwrite(&branch, 4, outf);
    }

    if (verbose) {
        fseek(outf, 0, SEEK_END);
        if (is_raw) printf("Saved processed image, %d -> %ld bytes\n", input_length, ftell(outf));
        else printf("Saved processed image, %ld bytes\n", ftell(outf));
    }

    free(input);
    fclose(outf);
    return 0;
}
