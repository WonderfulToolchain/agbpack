project('agbpack', 'c', default_options: 'c_std=gnu17,gnu11')

executable('agbpack', [
    'rt/out/crt0_multiboot_bin.c',
    'rt/out/crt0_rom_bin.c',
    'src/main.c',
    'vendor/apultra/src/expand.c',
    'vendor/apultra/src/matchfinder.c',
    'vendor/apultra/src/shrink.c',
    'vendor/apultra/src/libdivsufsort/lib/divsufsort.c',
    'vendor/apultra/src/libdivsufsort/lib/divsufsort.c',
    'vendor/apultra/src/libdivsufsort/lib/divsufsort_utils.c',
    'vendor/apultra/src/libdivsufsort/lib/sssort.c',
    'vendor/apultra/src/libdivsufsort/lib/trsort.c'
], include_directories: include_directories('rt/out', 'src', 'vendor/apultra/src', 'vendor/apultra/src/libdivsufsort/include'))
