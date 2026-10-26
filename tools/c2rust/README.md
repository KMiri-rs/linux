# c2rust binding workflow (UML/libc boundary)

This directory contains a small, reproducible workflow for translating a minimal C closure into Rust via `c2rust`.

## Reproduce `bitmap_c2rust.rs`

```bash
cd /home/zjp/KMiri/linux
./tools/c2rust/gen_c2rust_binding.sh \
  --name bitmap \
  --template bitmap_uml_libc \
  --out rust/kernel/bitmap_c2rust.rs
```

## Translate another binding closure

1. Prepare a minimal C file that only contains symbols used by your Rust module.
2. Run:

```bash
./tools/c2rust/gen_c2rust_binding.sh \
  --name <module> \
  --source /path/to/<module>_c2rust.c \
  --out rust/kernel/<module>_c2rust.rs
```

## Notes

- The script generates a local `compile_commands.json` in a temp workdir and runs:
  - `c2rust transpile compile_commands.json --emit-modules --overwrite-existing`
- It applies tiny normalization for in-kernel integration:
  - strips `#[no_mangle]`
  - turns `pub unsafe extern "C" fn` into `pub(crate) unsafe extern "C" fn`
  - prepends a permissive `#![allow(...)]` header useful in this repository
