#ifndef MP_HPP
#define MP_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <cstring>

#define MP_N64 4
#define MP_N   32

typedef uint64_t mp_uint_t[MP_N64];

static inline void mp_zero(uint64_t *r) {
#if MP_N64 == 4
    r[0] = 0;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
#else
    std::memset(r, 0, MP_N64 * sizeof(uint64_t));
#endif
}

static inline void mp_copy(uint64_t *r, const uint64_t *a) {
    if (r == a) return;

#if MP_N64 == 4
    const uintptr_t rp = reinterpret_cast<uintptr_t>(r);
    const uintptr_t ap = reinterpret_cast<uintptr_t>(a);
    static constexpr uintptr_t BYTES = MP_N64 * sizeof(uint64_t);

    if (rp > ap && rp < ap + BYTES) {
        r[3] = a[3];
        r[2] = a[2];
        r[1] = a[1];
        r[0] = a[0];
        return;
    }

    r[0] = a[0];
    r[1] = a[1];
    r[2] = a[2];
    r[3] = a[3];
#else
    std::memmove(r, a, MP_N64 * sizeof(uint64_t));
#endif
}

static inline int mp_cmp(const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    uint64_t ai;
    uint64_t bi;

    ai = a[3];
    bi = b[3];
    if (__builtin_expect(ai != bi, 1)) {
        return (ai > bi) ? 1 : -1;
    }

    ai = a[2];
    bi = b[2];
    if (__builtin_expect(ai != bi, 0)) {
        return (ai > bi) ? 1 : -1;
    }

    ai = a[1];
    bi = b[1];
    if (__builtin_expect(ai != bi, 0)) {
        return (ai > bi) ? 1 : -1;
    }

    ai = a[0];
    bi = b[0];
    if (__builtin_expect(ai != bi, 0)) {
        return (ai > bi) ? 1 : -1;
    }

    return 0;
#else
    for (int i = MP_N64 - 1; i >= 0; --i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }

    return 0;
#endif
}

static inline void mp_shl(uint64_t *r, const uint64_t *a, uint64_t k) {
    if (k >= (uint64_t)MP_N64 * 64u) {
        mp_zero(r);
        return;
    }
    if (k == 0) {
        mp_copy(r, a);
        return;
    }

    const auto wordShift = (uint32_t)(k >> 6);
    const auto bitShift  = (uint32_t)(k & 63u);

    if (bitShift == 0) {
        for (int i = MP_N64 - 1; i >= 0; --i) {
            int si = i - (int)wordShift;
            r[i] = si >= 0 ? a[si] : 0;
        }
        return;
    }

    const uint32_t inv = 64u - bitShift;
    for (int i = MP_N64 - 1; i >= 0; --i) {
        int si0 = i - (int)wordShift;
        int si1 = si0 - 1;

        uint64_t lo = (si0 >= 0) ? a[si0] : 0;
        uint64_t hi = (si1 >= 0) ? a[si1] : 0;

        r[i] = lo << bitShift | hi >> inv;
    }
}

static inline void mp_shr(uint64_t *r, const uint64_t *a, uint64_t k) {
    if (k >= (uint64_t)MP_N64 * 64u) {
        mp_zero(r);
        return;
    }
    if (k == 0) {
        mp_copy(r, a);
        return;
    }

    const auto wordShift = (uint32_t)(k >> 6);
    const auto bitShift  = (uint32_t)(k & 63u);

    if (bitShift == 0) {
        for (uint32_t i = 0; i < MP_N64; ++i) {
            uint32_t si = i + wordShift;
            r[i] = (si < (uint32_t)MP_N64) ? a[si] : 0;
        }
        return;
    }

    const uint32_t inv = 64u - bitShift;
    for (uint32_t i = 0; i < MP_N64; ++i) {
        uint32_t si0 = i + wordShift;
        uint32_t si1 = si0 + 1;

        uint64_t lo = (si0 < (uint32_t)MP_N64) ? a[si0] : 0;
        uint64_t hi = (si1 < (uint32_t)MP_N64) ? a[si1] : 0;

        r[i] = lo >> bitShift | hi << inv;
    }
}

static inline void mp_and(uint64_t *r, const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    r[0] = a[0] & b[0];
    r[1] = a[1] & b[1];
    r[2] = a[2] & b[2];
    r[3] = a[3] & b[3];
#else
    for (int i = 0; i < MP_N64; i++) r[i] = a[i] & b[i];
#endif
}

static inline void mp_or(uint64_t *r, const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    r[0] = a[0] | b[0];
    r[1] = a[1] | b[1];
    r[2] = a[2] | b[2];
    r[3] = a[3] | b[3];
#else
    for (int i = 0; i < MP_N64; i++) r[i] = a[i] | b[i];
#endif
}

static inline void mp_xor(uint64_t *r, const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    r[0] = a[0] ^ b[0];
    r[1] = a[1] ^ b[1];
    r[2] = a[2] ^ b[2];
    r[3] = a[3] ^ b[3];
#else
    for (int i = 0; i < MP_N64; i++) r[i] = a[i] ^ b[i];
#endif
}

static inline void mp_not(uint64_t *r, const uint64_t *a) {
#if MP_N64 == 4
    r[0] = ~a[0];
    r[1] = ~a[1];
    r[2] = ~a[2];
    r[3] = ~a[3];
#else
    for (int i = 0; i < MP_N64; i++) r[i] = ~a[i];
#endif
}

static inline bool mp_tstbit(const uint64_t *a, size_t bit) {
    if (bit >= (size_t)MP_N64 * 64u) return false;
    return a[bit >> 6] >> (bit & 63u) & 1ULL;
}

void     mp_set(uint64_t *r, uint64_t a);
bool     mp_is_zero(const uint64_t *a);

uint64_t mp_add(uint64_t *r, const uint64_t *a, const uint64_t *b);
uint64_t mp_add(uint64_t *r, const uint64_t *a, size_t an, const uint64_t *b, size_t bn);
uint64_t mp_sub(uint64_t *r, const uint64_t *a, const uint64_t *b);

uint64_t mp_add(uint64_t *r, const uint64_t *a, uint64_t b);
uint64_t mp_sub(uint64_t *r, const uint64_t *a, uint64_t b);

uint64_t mp_mul(uint64_t *r, const uint64_t *a, uint64_t b);
uint64_t mp_addmul(uint64_t *r, const uint64_t *a, size_t n, uint64_t b);

int32_t     mp_get_int32(const mp_uint_t a);
bool        mp_fits_int32(const uint64_t *a);
bool        mp_set(uint64_t *r, const char *str, uint32_t base);
std::string mp_get_str(const uint64_t *a, uint32_t base);

void mp_set_mod(uint64_t *r, int64_t a, const uint64_t *mod);
bool mp_set_mod(uint64_t *r, const char *str, uint32_t base, const uint64_t *mod);

void mp_export_be(uint8_t *r, const uint64_t *a);
void mp_import_be(uint64_t *r, const uint8_t *a);

void mp_div(uint64_t *q, uint64_t *r, const uint64_t *num, const uint64_t *den);
void mp_pow_mod(uint64_t *r, const uint64_t *base, const uint64_t *exp, const uint64_t *mod);
bool mp_inv_mod(uint64_t *r, const uint64_t *a, const uint64_t *mod);

#endif
