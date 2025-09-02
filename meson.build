project('agbpack', 'c', default_options: 'c_std=gnu11', version: '0.3.1')

executable('agbpack', [
    'rt/out/bootstrap_multiboot_bin.c',
    'rt/out/bootstrap_multiboot_nopack_bin.c',
    'rt/out/bootstrap_rom_bin.c',
    'rt/out/bootstrap_rom_nopack_bin.c',
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
