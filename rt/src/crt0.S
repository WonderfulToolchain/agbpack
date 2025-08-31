/**
 * Copyright (c) 2022 Luna Mittelbach
 * Copyright (c) 2023, 2025 Adrian "asie" Siekierka
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Originally from https://github.com/sdk-seven/runtime .
 * Modified for the Wonderful toolchain.
 */

.syntax         unified
.cpu            arm7tdmi

_header:
#ifdef MULTIBOOT
    b       _start_rom      @ ROM entry point
#else
    b       _start
#endif

    @ Logo Data (filled by post-link tool)
    .zero   156

    @ Checksum Area Start
    .zero   12              @ Game title
    .zero   4               @ Game code
    .zero   2               @ Maker code
    .byte   0x96            @ Mandatory
    .zero   1               @ Device code
    .zero   1               @ Device type
    .zero   7               @ Reserved
    .zero   1               @ Game version

    @ Checksum Area End
    .zero   1               @ Checksum (filled by post-link tool)
    .zero   2               @ Reserved

#ifdef MULTIBOOT
    b       _start          @ Multiboot entry point
    .zero   28
    b       _start          @ JOYBUS entry point
#else
    @ Fake multiboot entry point - infinite loop
    @ Required to pass mGBA's multiboot auto-detect
    b       .
#endif

#ifdef MULTIBOOT
@ If a multiboot image is booted from ROM, copy its contents to RAM.
_start_rom:
    and         r0, pc, 0xFF000000
    ldr         r1, =#0x2000000

    cmp         r1, r0
    @ Wrong entrypoint, continue.
    beq         _start
    @ Wanted ROM, got RAM, give up
    bhi         .
    @ Wanted RAM, got ROM, copy

    @ Calculate end of ROM
    adr         r2, _appended_data
    ldm         r2!, {r3}
    add         r2, r2, r3
    ldm         r2!, {r3}
    add         r2, r2, r3, lsl #2

    @ r0 = we loaded here
    @ r1 = we want to be here
    @ r2 = end
1:
    cmp         r0, r2
    bhs         1f
    ldm         r0!, {r4, r5, r6, r7, r8, r9, r10, r11}
    stm         r1!, {r4, r5, r6, r7, r8, r9, r10, r11}
    b           1b
1:
    @ Long branch to the real _start
    @ b _start would appear to work, but would continue
    @ running code in the wrong memory segment.
    ldr         pc, =#0x2000000
#endif

_start:
    @ Disable IRQs
    ldr         r0, =REG_IME
    str         r0, [r0]

    @ Initialize stack
    ldr		    sp, =0x3008000

    @ Relocate command stream to IWRAM
    adr         r0, _appended_data
    ldm         r0!, {r2}
    add         r0, r0, r2
    ldm         r0!, {r2}
    sub         r1, sp, #0x40
    sub         r1, r1, r2, lsl #2
    orr		    r2, r2, #(1 << 26)
    mov         r4, r1
    swi         11 << 16

    @ Relocate extraction code to IWRAM
    adr         r0, _extract_start
    adr         r2, _appended_data
    sub         r2, r2, r0
    sub         r1, r4, r2
    mov         r2, r2, lsr #2
    orr		    r2, r2, #(1 << 26)
    push        {r1}
    swi         11 << 16

    @ Jump to relocated extraction code
    pop         {r1}
    bx          r1         

    .pool

    @ r4 - command stream address
_extract_start:
    b           _extract

#include "apack.s"

    .pool

_extract:
    @ Extraction loop
1:
    @ Source address, Destination address, Length/Flags
    ldm         r4!, {r0, r1, r2}
    @ If source address == 0, treat destination address as jump target
    cmp         r0, 0
    bxeq        r1
    @ If bit 30 set, move data first, then use decompression
    tst         r2, #(1 << 30)
    bne         depack_move
    @ If bit 31 set, use decompression
    tst         r2, #(1 << 31)
    bne         depack
    @ If bit 29 set, use LZSS VRAM decompression
    tst         r2, #(1 << 29)
    swine       18 << 16
    @ Pass to GBA BIOS for copying/filling
    swieq       11 << 16
    b           1b
depack_move:
    bic         r2, r2, #0xF0000000
    add         r0, r0, r2
    mov         r3, #0x2040000
    sub         r2, r3, r2
    push        {r2, r4}
1:
    cmp         r2, r3
    bhs         1f
    ldmdb       r0!, {r4, r5, r6, r7, r8, r9, r10, r11}
    stmdb       r3!, {r4, r5, r6, r7, r8, r9, r10, r11}
    b           1b
1:
    pop         {r0, r4}
    b           depack

_appended_data:

.equiv          REG_IME,        0x04000208
