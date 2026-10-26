/*
 * How this file was produced
 * --------------------------
 * This file is a hand-written minimal C closure prepared for C2Rust, not a
 * direct copy of a single Linux source file.
 *
 * Steps used to derive it:
 * 1) Start from rust/kernel/bitmap.rs and enumerate the C symbols required by
 *    the Miri test path (allocation, bit set/clear, find operations, copy).
 * 2) Map those symbols to kernel-side semantics from bitmap/find helpers
 *    (e.g. include/linux/bitmap.h, include/linux/find.h, lib/find_bit.c,
 *    rust/helpers/bitops.c, rust/helpers/bitmap.c).
 * 3) Re-express only the needed behavior in a UML/libc boundary form so the
 *    code depends on libc primitives only (calloc/free/memcpy/memset).
 * 4) Keep the closure intentionally small and deterministic for C2Rust
 *    transpilation and KMiri execution.
 *
 * Intended use:
 * - Feed this file to tools/c2rust/gen_c2rust_binding.sh
 * - Generate rust/kernel/bitmap_c2rust.rs (or an equivalent output path)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline size_t bits_per_long(void) {
    return sizeof(unsigned long) * 8;
}

static inline size_t words_for_bits(size_t nbits) {
    return (nbits + bits_per_long() - 1) / bits_per_long();
}

unsigned long *bitmap_zalloc(unsigned int nbits, unsigned int flags) {
    (void)flags;
    size_t words = words_for_bits((size_t)nbits);
    if (words == 0) {
        words = 1;
    }
    return (unsigned long *)calloc(words, sizeof(unsigned long));
}

void bitmap_free(const unsigned long *bitmap) {
    free((void *)bitmap);
}

void __set_bit(unsigned long nr, unsigned long *addr) {
    size_t word = nr / bits_per_long();
    size_t bit = nr % bits_per_long();
    addr[word] |= (1UL << bit);
}

void __clear_bit(unsigned long nr, unsigned long *addr) {
    size_t word = nr / bits_per_long();
    size_t bit = nr % bits_per_long();
    addr[word] &= ~(1UL << bit);
}

void set_bit(unsigned long nr, volatile unsigned long *addr) {
    __set_bit(nr, (unsigned long *)addr);
}

void clear_bit(unsigned long nr, volatile unsigned long *addr) {
    __clear_bit(nr, (unsigned long *)addr);
}

unsigned long _find_next_bit(const unsigned long *addr, unsigned long nbits, unsigned long start) {
    for (unsigned long i = start; i < nbits; i++) {
        size_t word = i / bits_per_long();
        size_t bit = i % bits_per_long();
        if ((addr[word] & (1UL << bit)) != 0) {
            return i;
        }
    }
    return nbits;
}

unsigned long _find_next_zero_bit(const unsigned long *addr, unsigned long nbits, unsigned long start) {
    for (unsigned long i = start; i < nbits; i++) {
        size_t word = i / bits_per_long();
        size_t bit = i % bits_per_long();
        if ((addr[word] & (1UL << bit)) == 0) {
            return i;
        }
    }
    return nbits;
}

unsigned long _find_last_bit(const unsigned long *addr, unsigned long nbits) {
    if (nbits == 0) {
        return 0;
    }
    for (unsigned long i = nbits; i > 0; i--) {
        unsigned long bit = i - 1;
        size_t word = bit / bits_per_long();
        size_t off = bit % bits_per_long();
        if ((addr[word] & (1UL << off)) != 0) {
            return bit;
        }
    }
    return nbits;
}

void bitmap_copy_and_extend(unsigned long *to, const unsigned long *from, unsigned int count, unsigned int size) {
    size_t to_words = words_for_bits(size);
    size_t from_words = words_for_bits(count);
    memset(to, 0, to_words * sizeof(unsigned long));
    memcpy(to, from, from_words * sizeof(unsigned long));

    if (count < size) {
        size_t bit = count;
        size_t bit_in_long = bit % bits_per_long();
        if (bit_in_long != 0 && from_words > 0) {
            to[from_words - 1] &= ((1UL << bit_in_long) - 1);
        }
    }
}
