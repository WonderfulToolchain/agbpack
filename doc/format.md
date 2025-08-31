
Appended data format:

* bytes 0..3: ROM data length, in bytes
* bytes 4...: ROM data

Command stream format:

* bytes 0..3: command stream length, in words
* bytes 4...: command stream, 12 bytes per command
  * bytes 0..3: source
    * if source == 0, branch to destination
  * bytes 4..7: destination
  * bytes 8..11: flags
    * if bit 31 set, extract source to destination using aPLib
    * if bit 30 set, treat bits 0..23 as compressed length (multiple of 32), move source to end of EWRAM, then extract to destination using aPLib
    * if bit 29 set, extract source to destination using VRAM-safe BIOS LZ (SWI 0x12)
    * otherwise, treat as a BIOS memory copy/fill command (SWI 0xB)

The last four bytes are the offset (negative!) to the command stream length, in bytes.
They are not used by the extraction code, so they can be used to update the command stream at runtime.
