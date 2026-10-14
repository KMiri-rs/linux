#!/bin/sh
# Generates .config for the miri build. Run only via makefile.local's
# `.config` target, which invokes it when this script is newer than .config.
set -eu

export ARCH=x86_64
export LLVM=1

make rustavailable
make LLVM=1 allnoconfig
./scripts/config --enable RUST
./scripts/config --enable MODULES
./scripts/config --enable SAMPLES
./scripts/config --enable SAMPLES_RUST
./scripts/config --module SAMPLE_RUST_MINIMAL
./scripts/config --enable KUNIT
# Enable serial output for qemu.
./scripts/config --enable PRINTK
./scripts/config --enable SERIAL_8250
./scripts/config --enable SERIAL_8250_CONSOLE
# Enable initramfs decompression for qemu.
./scripts/config --enable BLK_DEV_INITRD
./scripts/config --enable RD_GZIP
./scripts/config --enable DEVTMPFS
./scripts/config --enable DEVTMPFS_MOUNT
# allnoconfig may disable executable format loaders; without these,
# /init or /bin/sh in initramfs fails with "error -8" (ENOEXEC).
./scripts/config --enable BINFMT_ELF
./scripts/config --enable BINFMT_SCRIPT
make olddefconfig
make savedefconfig
