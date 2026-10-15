// SPDX-License-Identifier: GPL-2.0

//! Generates Miri tests from saved `rustdoc`-generated tests.

use std::{
    fs,
    fs::File,
    io::{
        BufWriter,
        Read,
        Write, //
    },
};

fn main() {
    let only_file = std::env::var("MIRI_DOCTEST_FILTER").ok();

    let mut paths = fs::read_dir("rust/test/doctests/kernel")
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .collect::<Vec<_>>();

    // Sort paths.
    paths.sort();

    let mut rust_tests = String::new();
    let mut body = String::new();
    let mut last_file = String::new();
    let mut number = 0;
    for path in paths {
        // The `name` follows the `{file}_{line}_{number}` pattern (see description in
        // `scripts/rustdoc_test_builder.rs`). Discard the `number`.
        let name = path.file_name().unwrap().to_str().unwrap().to_string();

        // Extract the `file`, discarding `line` and the generated `number`.
        let file = name.rsplit_once('_').unwrap().0.rsplit_once('_').unwrap().0;

        if only_file
            .as_deref()
            .is_some_and(|candidate| candidate != file)
        {
            continue;
        }

        // Generate an ID sequence ("test number") for each doctest in the file.
        if file == last_file {
            number += 1;
        } else {
            number = 0;
            last_file = file.to_string();
        }

        let test_name = format!("rust_doctest_kernel_{file}_{number}");

        // Read the test's text contents to dump it below.
        body.clear();
        File::open(path).unwrap().read_to_string(&mut body).unwrap();

        use std::fmt::Write;
        write!(
            rust_tests,
            r#"#[test]
fn {test_name}() {{
    {{
        #![allow(unreachable_pub, clippy::disallowed_names)]
        {body}
        main();
    }}
}}

"#
        )
        .unwrap();
    }

    let rust_tests = rust_tests.trim();

    write!(
        BufWriter::new(File::create("rust/doctests_kernel_generated_miri.rs").unwrap()),
        r#"//! `kernel` crate documentation tests for Miri.

/// Dummy module type for doctest context.
struct LocalModule;

use core::{{
    ffi::{{
        c_int,
        c_ulong,
        c_void,
    }},
    ptr::null_mut,
}};
use kernel::{{
    prelude::*,
    str::CStr,
    ModuleMetadata,
    ThisModule, //
}};
use std::dbg;

unsafe extern "C" {{
    fn free(ptr: *mut c_void);
    fn realloc(ptr: *mut c_void, size: usize) -> *mut c_void;
}}

unsafe fn rust_helper_realloc_like(ptr: *const c_void, new_size: usize) -> *mut c_void {{
    let raw_ptr = ptr.cast_mut();

    if new_size == 0 {{
        if !raw_ptr.is_null() {{
            unsafe {{ free(raw_ptr) }};
        }}
        return null_mut();
    }}

    unsafe {{ realloc(raw_ptr, new_size) }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_krealloc_node_align(
    ptr: *const c_void,
    new_size: usize,
    _align: c_ulong,
    _flags: u32,
    _nid: c_int,
) -> *mut c_void {{
    unsafe {{ rust_helper_realloc_like(ptr, new_size) }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_vrealloc_node_align(
    ptr: *const c_void,
    new_size: usize,
    _align: c_ulong,
    _flags: u32,
    _nid: c_int,
) -> *mut c_void {{
    unsafe {{ rust_helper_realloc_like(ptr, new_size) }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_kvrealloc_node_align(
    ptr: *const c_void,
    new_size: usize,
    _align: c_ulong,
    _flags: u32,
    _nid: c_int,
) -> *mut c_void {{
    unsafe {{ rust_helper_realloc_like(ptr, new_size) }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_REFCOUNT_INIT(n: c_int) -> kernel::bindings::refcount_t {{
    kernel::bindings::refcount_t {{
        refs: kernel::bindings::atomic_t {{ counter: n }},
    }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_refcount_inc(r: *mut kernel::bindings::refcount_t) {{
    unsafe {{
        (*r).refs.counter += 1;
    }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_refcount_dec(r: *mut kernel::bindings::refcount_t) {{
    unsafe {{
        (*r).refs.counter -= 1;
    }}
}}

#[no_mangle]
pub unsafe extern "C" fn rust_helper_refcount_dec_and_test(
    r: *mut kernel::bindings::refcount_t,
) -> kernel::bindings::bool_ {{
    unsafe {{
        (*r).refs.counter -= 1;
        (*r).refs.counter == 0
    }}
}}

impl ModuleMetadata for LocalModule {{
    const NAME: &'static CStr = c"rust_doctests_kernel";
    const THIS_MODULE: ThisModule = {{
        // SAFETY: `try_module_get`/`module_put` handle null module pointers gracefully.
        unsafe {{ ThisModule::from_ptr(null_mut()) }}
    }};
}}

{rust_tests}
"#
    )
    .unwrap();
}
