# agbpack

Tool for compressing GBA images using aPlib, with a particular focus on multiboot images.

## Usage

### Basic usage

    $ agbpack original_mb.gba packed_mb.gba

### Advanced usage

Applying `agbpack` to an otherwise unmodified `.gba` only makes for a smaller `.gba` file. The power of `agbpack` - breaking the 256 KiB uncompressed binary size limit - comes with using it as a relocated ELF loader. In a typical workflow, this can be achieved by applying it as a replacement for `objcopy`:

    $ objcopy -O binary image.elf image.gba

becomes:

    $ agbpack image.elf image.gba

Of course, the resulting image still requires header fixing with `wf-gbatool fix`, `gbafix`, or a similar tool.

Note that most link scripts and startup code in toolchains are written with the assumption that everything lands in EWRAM. As such, without further adjustments, this is effectively identical to just converting a `.gba` file. To get the full effect, adjust the crt0 and linker script so that:

* EWRAM, IWRAM and VRAM data sections get their own, separate PHDRs and are *not* relocated by the crt0,
* EWRAM, IWRAM and VRAM BSS sections get their own, separate PHDRs and are *not* cleared by the crt0,
* the GBA header is removed from the ELF image - `agbpack` parsses the ELF entrypoint directly.

With this approach, the agbpack bootstrap will directly extract compressed data to EWRAM, IWRAM and VRAM. This allows the sum of uncompressed data to be larger than 256 KiB.

## Limitations

* For multiboot images:
  * About 1 KiB of space is reserved at the end of IWRAM to store the decompression routine.
  * To achieve more optimalcompression results, it is recommended not to use up the entirely of EWRAM - this reduces the size of the compression window. BSS (zero-filled) data does not count towards this limit.
* Cartridge images are only supported as `.elf` files, not as `.gba `files.

## License

The `agbpack` compression tool, by itself, is MIT-licensed. It also includes the following additional code:

* Unmodified zlib-licensed code from the compressor [aPultra](https://github.com/emmanuel-marty/apultra) by Emmanuel Marty,
* MIT-licensed code from the library libdivsufsort by Yuta Mori.

The runtime code appended to executables is zlib-licensed. It also includes the aPlib decompression routine by Dan Weiss, [placed in the public domain](https://github.com/emmanuel-marty/apultra/pull/2). As such, no additional licensing requirements are placed on distribution of binaries created using agbpack.
