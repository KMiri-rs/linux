#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage:
  $0 --name <name> [--template <template>] [--source <c_file>] [--out <rust_file>] [--workdir <dir>] [--clang <cmd>]

Examples:
  # Reproduce bitmap_c2rust.rs from the built-in UML/libc template
  $0 --name bitmap --template bitmap_uml_libc --out rust/kernel/bitmap_c2rust.rs

  # Transpile a custom minimal C closure
  $0 --name idpool --source /tmp/idpool_c2rust.c --out rust/kernel/idpool_c2rust.rs

Options:
  --name      Logical name (used for generated C file naming)
  --template  Template name under tools/c2rust/templates without .c suffix
  --source    Path to custom C source (if set, template is ignored)
  --out       Output Rust file path (default: <workdir>/<name>_c2rust.rs)
  --workdir   Working directory (default: /tmp/c2rust-<name>)
  --clang     Compile command prefix used in compile_commands.json
              (default: clang)
USAGE
}

name=""
template=""
source_file=""
out_file=""
workdir=""
clang_bin="clang"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) name="$2"; shift 2 ;;
    --template) template="$2"; shift 2 ;;
    --source) source_file="$2"; shift 2 ;;
    --out) out_file="$2"; shift 2 ;;
    --workdir) workdir="$2"; shift 2 ;;
    --clang) clang_bin="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ -z "$name" ]]; then
  echo "--name is required" >&2
  usage
  exit 2
fi

if ! command -v c2rust >/dev/null 2>&1; then
  echo "c2rust is not in PATH" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ -z "$workdir" ]]; then
  workdir="/tmp/c2rust-${name}"
fi
mkdir -p "$workdir"

c_file="$workdir/${name}_c2rust.c"
if [[ -n "$source_file" ]]; then
  cp "$source_file" "$c_file"
else
  if [[ -z "$template" ]]; then
    echo "Either --source or --template is required" >&2
    usage
    exit 2
  fi
  template_file="$repo_root/tools/c2rust/templates/${template}.c"
  if [[ ! -f "$template_file" ]]; then
    echo "Template not found: $template_file" >&2
    exit 1
  fi
  cp "$template_file" "$c_file"
fi

cat > "$workdir/compile_commands.json" <<JSON
[
  {
    "directory": "$workdir",
    "command": "$clang_bin -c $(basename "$c_file") -o ${name}_c2rust.o",
    "file": "$(basename "$c_file")"
  }
]
JSON

(
  cd "$workdir"
  c2rust transpile compile_commands.json --emit-modules --overwrite-existing
)

generated_rs="$workdir/${name}_c2rust.rs"
if [[ ! -f "$generated_rs" ]]; then
  echo "c2rust did not produce expected file: $generated_rs" >&2
  exit 1
fi

# Minimal normalization for in-kernel module usage.
# 1) strip #[no_mangle]
# 2) use pub(crate) visibility for C-exported fns
perl -i -0pe 's/^#\[no_mangle\]\n//mg; s/^pub unsafe extern "C" fn /pub(crate) unsafe extern "C" fn /mg' "$generated_rs"

allow_header='// SPDX-License-Identifier: GPL-2.0

#![allow(
    non_camel_case_types,
    non_snake_case,
    unused_mut,
    dead_code,
    unsafe_op_in_unsafe_fn,
    clippy::missing_safety_doc,
    clippy::not_unsafe_ptr_arg_deref
)]

'

if ! rg -q "unsafe_op_in_unsafe_fn" "$generated_rs"; then
  tmp_file="$workdir/.tmp.${name}_c2rust.rs"
  printf "%b" "$allow_header" > "$tmp_file"
  cat "$generated_rs" >> "$tmp_file"
  mv "$tmp_file" "$generated_rs"
fi

if [[ -z "$out_file" ]]; then
  out_file="$generated_rs"
else
  mkdir -p "$(dirname "$out_file")"
  cp "$generated_rs" "$out_file"
fi

echo "[ok] c2rust source: $c_file"
echo "[ok] compile db: $workdir/compile_commands.json"
echo "[ok] rust output: $out_file"
