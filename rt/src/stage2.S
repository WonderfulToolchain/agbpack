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

    @ r4 - command stream address
_extract:
    @ Extraction loop
1:
    @ Source address, Destination address, Length/Flags
    ldm         r4!, {r0, r1, r2}
    @ If source address == 0, treat destination address as jump target
    cmp         r0, 0
    bxeq        r1
#ifdef APLIB
#ifdef MULTIBOOT
    @ If bit 30 set, move data first, then use decompression
    tst         r2, #(1 << 30)
    bne         depack_move
#endif
    @ If bit 31 set, use decompression
    tst         r2, #(1 << 31)
    bne         depack
#else
    @ If bit 31 set, use LZSS WRAM decompression
    tst         r2, #(1 << 31)
    swine       17 << 16
#endif
    @ If bit 29 set, use LZSS VRAM decompression
    tst         r2, #(1 << 29)
    swine       18 << 16
    @ Pass to GBA BIOS for copying/filling
    swieq       11 << 16
    b           1b
#ifdef APLIB
#ifdef MULTIBOOT
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
#endif
#endif

    .pool

#ifdef APLIB
#include "apack.s"

    .pool
#endif
