# agbpack

Tool for compressing GBA multiboot images using aPlib.

## Usage

### Basic usage

`agbpack` essentially replaces `objcopy`:

   $ agbpack image.elf image.gba

Note that the resulting image requires header fixing with `wf-gbatool fix`, `gbafix`, or a similar tool.

### Advanced usage

Of course, applying `agbpack` to an otherwise unmodified GBA `.elf` only makes for a smaller `.gba` file - useful for demosceners, not so much for creating larger multiboot programs.

For best results, adjust the link script and crt0 of your project so that:

* all data and BSS sections targetted at EWRAM, IWRAM and VRAM are placed in those sections in the ELF and not relocated by the crt0,
* the GBA header is removed from the ELF image.

`agbpack` supports copying and zeroing arbitrary memory sections, as well as parsing the ELF entrypoint. This allows directly unpacking compressed data to EWRAM and VRAM and, in doing so, creating multiboot programs larger than 256 KiB.

## License

The `agbpack` compression tool, by itself, is MIT-licensed. It also includes the following additional code:

* Unmodified zlib-licensed code from the compressor [aPultra](https://github.com/emmanuel-marty/apultra) by Emmanuel Marty,
* MIT-licensed code from the library libdivsufsort by Yuta Mori.

THe runtime code appended to executables is zlib-licensed. It also includes the aPlib decompression routine by Dan Weiss, [placed in the public domain](https://github.com/emmanuel-marty/apultra/pull/2). As such, no additional licensing requirements are placed on distribution of binaries created using agbpack.
