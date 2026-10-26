// SPDX-License-Identifier: GPL-2.0

#![allow(
    non_camel_case_types,
    non_snake_case,
    unused_mut,
    dead_code,
    unsafe_op_in_unsafe_fn,
    clippy::missing_safety_doc,
    clippy::not_unsafe_ptr_arg_deref
)]

// Generated with:
// c2rust transpile compile_commands.json --emit-modules --overwrite-existing
// from a UML/libc-oriented bitmap helper C file used by Miri tests.

unsafe extern "C" {
    fn calloc(__nmemb: size_t, __size: size_t) -> *mut ::core::ffi::c_void;
    fn free(__ptr: *mut ::core::ffi::c_void);
    fn memcpy(
        __dest: *mut ::core::ffi::c_void,
        __src: *const ::core::ffi::c_void,
        __n: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn memset(
        __s: *mut ::core::ffi::c_void,
        __c: ::core::ffi::c_int,
        __n: size_t,
    ) -> *mut ::core::ffi::c_void;
}

type size_t = usize;

#[inline]
unsafe extern "C" fn bits_per_long() -> size_t {
    (::core::mem::size_of::<::core::ffi::c_ulong>() as size_t).wrapping_mul(8)
}

#[inline]
unsafe extern "C" fn words_for_bits(mut nbits: size_t) -> size_t {
    nbits
        .wrapping_add(bits_per_long())
        .wrapping_sub(1)
        .wrapping_div(bits_per_long())
}

pub(crate) unsafe extern "C" fn bitmap_zalloc(
    mut nbits: ::core::ffi::c_uint,
    mut _flags: ::core::ffi::c_uint,
) -> *mut ::core::ffi::c_ulong {
    let mut words: size_t = words_for_bits(nbits as size_t);
    if words == 0 {
        words = 1;
    }

    calloc(words, ::core::mem::size_of::<::core::ffi::c_ulong>() as size_t)
        as *mut ::core::ffi::c_ulong
}

pub(crate) unsafe extern "C" fn bitmap_free(mut bitmap: *const ::core::ffi::c_ulong) {
    free(bitmap as *mut ::core::ffi::c_void);
}

pub(crate) unsafe extern "C" fn __set_bit(
    mut nr: ::core::ffi::c_ulong,
    mut addr: *mut ::core::ffi::c_ulong,
) {
    let word: size_t = (nr as size_t).wrapping_div(bits_per_long());
    let bit: size_t = (nr as size_t).wrapping_rem(bits_per_long());
    *addr.offset(word as isize) |= (1 as ::core::ffi::c_ulong) << bit;
}

pub(crate) unsafe extern "C" fn __clear_bit(
    mut nr: ::core::ffi::c_ulong,
    mut addr: *mut ::core::ffi::c_ulong,
) {
    let word: size_t = (nr as size_t).wrapping_div(bits_per_long());
    let bit: size_t = (nr as size_t).wrapping_rem(bits_per_long());
    *addr.offset(word as isize) &= !((1 as ::core::ffi::c_ulong) << bit);
}

pub(crate) unsafe extern "C" fn set_bit(
    mut nr: ::core::ffi::c_ulong,
    mut addr: *mut ::core::ffi::c_ulong,
) {
    __set_bit(nr, addr as *mut ::core::ffi::c_ulong);
}

pub(crate) unsafe extern "C" fn clear_bit(
    mut nr: ::core::ffi::c_ulong,
    mut addr: *mut ::core::ffi::c_ulong,
) {
    __clear_bit(nr, addr as *mut ::core::ffi::c_ulong);
}

pub(crate) unsafe extern "C" fn _find_next_bit(
    mut addr: *const ::core::ffi::c_ulong,
    mut nbits: ::core::ffi::c_ulong,
    mut start: ::core::ffi::c_ulong,
) -> ::core::ffi::c_ulong {
    let mut i: ::core::ffi::c_ulong = start;
    while i < nbits {
        let word: size_t = (i as size_t).wrapping_div(bits_per_long());
        let bit: size_t = (i as size_t).wrapping_rem(bits_per_long());
        if *addr.offset(word as isize) & (1 as ::core::ffi::c_ulong) << bit != 0 {
            return i;
        }
        i = i.wrapping_add(1);
    }
    nbits
}

pub(crate) unsafe extern "C" fn _find_next_zero_bit(
    mut addr: *const ::core::ffi::c_ulong,
    mut nbits: ::core::ffi::c_ulong,
    mut start: ::core::ffi::c_ulong,
) -> ::core::ffi::c_ulong {
    let mut i: ::core::ffi::c_ulong = start;
    while i < nbits {
        let word: size_t = (i as size_t).wrapping_div(bits_per_long());
        let bit: size_t = (i as size_t).wrapping_rem(bits_per_long());
        if *addr.offset(word as isize) & (1 as ::core::ffi::c_ulong) << bit == 0 {
            return i;
        }
        i = i.wrapping_add(1);
    }
    nbits
}

pub(crate) unsafe extern "C" fn _find_last_bit(
    mut addr: *const ::core::ffi::c_ulong,
    mut nbits: ::core::ffi::c_ulong,
) -> ::core::ffi::c_ulong {
    if nbits == 0 {
        return 0;
    }

    let mut i: ::core::ffi::c_ulong = nbits;
    while i > 0 {
        let bit: ::core::ffi::c_ulong = i.wrapping_sub(1);
        let word: size_t = (bit as size_t).wrapping_div(bits_per_long());
        let off: size_t = (bit as size_t).wrapping_rem(bits_per_long());
        if *addr.offset(word as isize) & (1 as ::core::ffi::c_ulong) << off != 0 {
            return bit;
        }
        i = i.wrapping_sub(1);
    }

    nbits
}

pub(crate) unsafe extern "C" fn bitmap_copy_and_extend(
    mut to: *mut ::core::ffi::c_ulong,
    mut from: *const ::core::ffi::c_ulong,
    mut count: ::core::ffi::c_uint,
    mut size: ::core::ffi::c_uint,
) {
    let to_words: size_t = words_for_bits(size as size_t);
    let from_words: size_t = words_for_bits(count as size_t);

    memset(
        to as *mut ::core::ffi::c_void,
        0,
        to_words.wrapping_mul(::core::mem::size_of::<::core::ffi::c_ulong>() as size_t),
    );

    memcpy(
        to as *mut ::core::ffi::c_void,
        from as *const ::core::ffi::c_void,
        from_words.wrapping_mul(::core::mem::size_of::<::core::ffi::c_ulong>() as size_t),
    );

    if count < size {
        let bit: size_t = count as size_t;
        let bit_in_long: size_t = bit.wrapping_rem(bits_per_long());
        if bit_in_long != 0 && from_words > 0 {
            *to.offset(from_words.wrapping_sub(1) as isize) &=
                ((1 as ::core::ffi::c_ulong) << bit_in_long).wrapping_sub(1);
        }
    }
}
