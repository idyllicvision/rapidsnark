#include "mp.hpp"

#include <cstdint>
#include <string>
#include <climits>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <vector>

static inline uint64_t add_carry(uint64_t *out, uint64_t a, uint64_t b, uint64_t c) {
#if defined(__clang__) || defined(__GNUC__)
    uint64_t t;
    unsigned c1 = __builtin_add_overflow(a, b, &t);
    unsigned c2 = __builtin_add_overflow(t, c, &t);
    *out = t;
    return (c1 | c2);
#else
    uint64_t t0 = a + b;
    uint64_t carry1 = (t0 < a);
    uint64_t t1 = t0 + c;
    uint64_t carry2 = (t1 < t0);
    *out = t1;
    return carry1 | carry2;
#endif
}

static inline uint64_t sub_borrow(uint64_t *out, uint64_t a, uint64_t b, uint64_t c) {
#if defined(__clang__) || defined(__GNUC__)
    uint64_t t;
    unsigned b1 = __builtin_sub_overflow(a, b, &t);
    unsigned b2 = __builtin_sub_overflow(t, c, &t);
    *out = t;
    return (b1 | b2);
#else
    uint64_t t0 = a - b;
    uint64_t borrow1 = (a < b);
    uint64_t t1 = t0 - c;
    uint64_t borrow2 = (t0 < c);
    *out = t1;
    return borrow1 | borrow2;
#endif
}

void mp_set(uint64_t *r, uint64_t a) {
#if MP_N64 == 4
    r[0] = a;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
#else
    r[0] = a;
    for (int i = 1; i < MP_N64; i++) r[i] = 0;
#endif
}

bool mp_is_zero(const uint64_t *a) {
#if MP_N64 == 4
    return (a[0] | a[1] | a[2] | a[3]) == 0;
#else
    uint64_t acc = 0;
    for (int i = 0; i < MP_N64; ++i)
        acc |= a[i];
    return acc == 0;
#endif
}

uint64_t mp_add(uint64_t *r, const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    uint64_t c = 0;
    c = add_carry(&r[0], a[0], b[0], c);
    c = add_carry(&r[1], a[1], b[1], c);
    c = add_carry(&r[2], a[2], b[2], c);
    c = add_carry(&r[3], a[3], b[3], c);
    return c;
#else
    uint64_t c = 0;

    for (int i = 0; i < MP_N64; ++i) {
        c = add_carry(&r[i], a[i], b[i], c);
    }

    return c;
#endif
}

uint64_t mp_add(uint64_t *r, const uint64_t *a, uint64_t b) {
#if MP_N64 == 4
    uint64_t c = 0;
    c = add_carry(&r[0], a[0], b, c);
    c = add_carry(&r[1], a[1], 0, c);
    c = add_carry(&r[2], a[2], 0, c);
    c = add_carry(&r[3], a[3], 0, c);
    return c;
#else
    uint64_t c = add_carry(&r[0], a[0], b, 0);

    for (int i = 1; i < MP_N64; ++i) {
        c = add_carry(&r[i], a[i], 0, c);
    }

    return c;
#endif
}

uint64_t mp_sub(uint64_t *r, const uint64_t *a, const uint64_t *b) {
#if MP_N64 == 4
    uint64_t br = 0;
    br = sub_borrow(&r[0], a[0], b[0], br);
    br = sub_borrow(&r[1], a[1], b[1], br);
    br = sub_borrow(&r[2], a[2], b[2], br);
    br = sub_borrow(&r[3], a[3], b[3], br);
    return br;
#else
    uint64_t br = 0;

    for (int i = 0; i < MP_N64; ++i) {
        br = sub_borrow(&r[i], a[i], b[i], br);
    }

    return br;
#endif
}

uint64_t mp_sub(uint64_t *r, const uint64_t *a, uint64_t b) {
#if MP_N64 == 4
    uint64_t br = 0;
    br = sub_borrow(&r[0], a[0], b, br);
    br = sub_borrow(&r[1], a[1], 0, br);
    br = sub_borrow(&r[2], a[2], 0, br);
    br = sub_borrow(&r[3], a[3], 0, br);
    return br;
#else
    uint64_t br = sub_borrow(&r[0], a[0], b, 0);

    for (int i = 1; i < MP_N64; ++i) {
        br = sub_borrow(&r[i], a[i], 0, br);
    }

    return br;
#endif
}

static inline uint64_t load_be64(const uint8_t *p) {
    uint64_t x;
    std::memcpy(&x, p, sizeof(x));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(x);
#else
    return x;
#endif
}

static inline void store_be64(uint8_t *p, uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    x = __builtin_bswap64(x);
#endif
    std::memcpy(p, &x, sizeof(x));
}

void mp_import_be(uint64_t *r, const uint8_t *a) {
#if MP_N64 == 4
    r[3] = load_be64(a + 0);
    r[2] = load_be64(a + 8);
    r[1] = load_be64(a + 16);
    r[0] = load_be64(a + 24);
#else
    auto *dst = reinterpret_cast<uint8_t *>(r);
    const size_t nbytes = MP_N64 * sizeof(uint64_t);

    for (size_t i = 0; i < nbytes; ++i) {
        dst[i] = a[nbytes - 1 - i];
    }
#endif
}

void mp_export_be(uint8_t *r, const uint64_t *a) {
#if MP_N64 == 4
    store_be64(r + 0,  a[3]);
    store_be64(r + 8,  a[2]);
    store_be64(r + 16, a[1]);
    store_be64(r + 24, a[0]);
#else
    const auto *p = reinterpret_cast<const uint8_t *>(a);
    const size_t nbytes = MP_N64 * sizeof(uint64_t);

    for (size_t i = 0; i < nbytes; ++i) {
        r[i] = p[nbytes - 1 - i];
    }
#endif
}

enum class MpHexParseStatus {
    Ok,
    Invalid,
    Overflow
};

alignas(64) static constexpr int8_t MP_HEX_DIGIT_TABLE[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static inline int mp_hex_digit(unsigned char c) {
    return (int)MP_HEX_DIGIT_TABLE[c];
}

static inline bool mp_parse_hex64_exact_u256(uint64_t *r, const char *p) {
    uint64_t r3 = 0;
    uint64_t r2 = 0;
    uint64_t r1 = 0;
    uint64_t r0 = 0;

    for (int i = 0; i < 16; ++i) {
        const int d = mp_hex_digit((unsigned char)p[i]);
        if (d < 0) return false;
        r3 = (r3 << 4) | (uint64_t)d;
    }

    for (int i = 16; i < 32; ++i) {
        const int d = mp_hex_digit((unsigned char)p[i]);
        if (d < 0) return false;
        r2 = (r2 << 4) | (uint64_t)d;
    }

    for (int i = 32; i < 48; ++i) {
        const int d = mp_hex_digit((unsigned char)p[i]);
        if (d < 0) return false;
        r1 = (r1 << 4) | (uint64_t)d;
    }

    for (int i = 48; i < 64; ++i) {
        const int d = mp_hex_digit((unsigned char)p[i]);
        if (d < 0) return false;
        r0 = (r0 << 4) | (uint64_t)d;
    }

    r[3] = r3;
    r[2] = r2;
    r[1] = r1;
    r[0] = r0;
    return true;
}

static inline bool mp_parse_hex_chunk_u64(
    uint64_t *out,
    const char *begin,
    const char *end
) {
    uint64_t x = 0;

    for (const char *p = begin; p != end; ++p) {
        const int d = mp_hex_digit((unsigned char)*p);
        if (d < 0) {
            return false;
        }

        x = (x << 4) | (uint64_t)d;
    }

    *out = x;
    return true;
}

static MpHexParseStatus mp_parse_hex_u256(
    uint64_t *r,
    const char *str,
    bool allow_neg,
    bool *neg_out
) {
    mp_zero(r);

    while (*str && std::isspace((unsigned char)*str)) {
        ++str;
    }

    bool neg = false;

    if (*str == '+') {
        ++str;
    } else if (*str == '-') {
        if (!allow_neg) {
            return MpHexParseStatus::Invalid;
        }

        neg = true;
        ++str;
    }

#if MP_N64 == 4
    {
        const char *p = str;

        size_t len = 0;
        while (len <= 64u) {
            const unsigned char c = (unsigned char)p[len];

            if (c == '\0' || std::isspace(c)) {
                break;
            }

            ++len;
        }

        if (len == 64u) {
            const char *tail = p + 64;

            while (*tail && std::isspace((unsigned char)*tail)) {
                ++tail;
            }

            if (*tail == '\0') {
                if (mp_parse_hex64_exact_u256(r, p)) {
                    if (neg_out) {
                        *neg_out = neg;
                    }

                    return MpHexParseStatus::Ok;
                }
            }
        }
    }
#endif

    const char *digits = str;
    const char *end = str;

    while (*end && !std::isspace((unsigned char)*end)) {
        ++end;
    }

    if (end == digits) {
        return MpHexParseStatus::Invalid;
    }

    const char *tail = end;
    while (*tail && std::isspace((unsigned char)*tail)) {
        ++tail;
    }

    if (*tail != '\0') {
        return MpHexParseStatus::Invalid;
    }

    uint64_t tmp[MP_N64] = {};
    size_t pos = (size_t)(end - digits);

    for (int limb = 0; limb < MP_N64 && pos > 0; ++limb) {
        const size_t chunk_len = pos > 16u ? 16u : pos;

        const char *chunk_begin = digits + pos - chunk_len;
        const char *chunk_end   = digits + pos;

        if (!mp_parse_hex_chunk_u64(&tmp[limb], chunk_begin, chunk_end)) {
            return MpHexParseStatus::Invalid;
        }

        pos -= chunk_len;
    }

    bool overflow = false;

    while (pos > 0) {
        const int d = mp_hex_digit((unsigned char)digits[--pos]);

        if (d < 0) {
            return MpHexParseStatus::Invalid;
        }

        if (d != 0) {
            overflow = true;
        }
    }

    for (int i = 0; i < MP_N64; ++i) {
        r[i] = tmp[i];
    }

    if (neg_out) {
        *neg_out = neg;
    }

    return overflow ? MpHexParseStatus::Overflow : MpHexParseStatus::Ok;
}

bool mp_set(uint64_t *r, const char *str, uint32_t base) {
    if (base == 0u) base = 10u;
    if (base < 2u || base > 16u) return false;

    if (base == 16u) {
        MpHexParseStatus st = mp_parse_hex_u256(r, str, false, nullptr);
        return st == MpHexParseStatus::Ok;
    }

    mp_zero(r);

    while (*str && std::isspace((unsigned char)*str)) str++;
    if (*str == '+') str++;
    if (*str == '-') return false;

    bool any = false;

    for (; *str; ++str) {
        if (std::isspace((unsigned char)*str)) break;

        const unsigned char uc = (unsigned char)*str;
        uint32_t digit;
        if (uc >= (unsigned char)'0' && uc <= (unsigned char)'9') {
            digit = (uint32_t)(uc - (unsigned char)'0');
        } else if (uc >= (unsigned char)'a' && uc <= (unsigned char)'f') {
            digit = (uint32_t)(uc - (unsigned char)'a') + 10u;
        } else if (uc >= (unsigned char)'A' && uc <= (unsigned char)'F') {
            digit = (uint32_t)(uc - (unsigned char)'A') + 10u;
        } else {
            return false;
        }
        if (digit >= base) return false;

        if (mp_mul(r, r, base) != 0) return false;
        if (mp_add(r, r, digit) != 0) return false;
        any = true;
    }

    if (!any) return false;
    while (*str && std::isspace((unsigned char)*str)) str++;
    return *str == '\0';
}

static inline uint32_t mp_div(uint64_t *q, const uint64_t *a, uint32_t base) {
#if defined(__SIZEOF_INT128__)
    uint64_t rem = 0;

    for (int i = MP_N64 - 1; i >= 0; i--) {
        __uint128_t cur = (((__uint128_t)rem) << 2*MP_N) | (__uint128_t)a[i];
        q[i] = (uint64_t)(cur / base);
        rem  = (uint64_t)(cur % base);
    }
    return (uint32_t)rem;
#else
    // long division in base 2^32: numerator = rem*2^64 + a[i]
    // rem < base <= 16 => safe in uint32_t
    uint32_t rem = 0;
    for (int i = MP_N64 - 1; i >= 0; --i) {
        uint32_t hi = (uint32_t)(a[i] >> MP_N);
        uint32_t lo = (uint32_t)(a[i] & 0xFFFFFFFFu);

        uint64_t cur = ((uint64_t)rem << MP_N) | (uint64_t)hi;
        uint32_t qhi = (uint32_t)(cur / base);
        rem = (uint32_t)(cur % base);

        cur = ((uint64_t)rem << MP_N) | (uint64_t)lo;
        uint32_t qlo = (uint32_t)(cur / base);
        rem = (uint32_t)(cur % base);

        q[i] = ((uint64_t)qhi << MP_N) | (uint64_t)qlo;
    }
    return rem;
#endif
}

#if defined(__SIZEOF_INT128__)
static inline uint64_t mp_div_u64_base(uint64_t *q, const uint64_t *a, uint64_t base) {
    __uint128_t rem = 0;

    for (int i = MP_N64 - 1; i >= 0; --i) {
        __uint128_t cur = (rem << 64) | (__uint128_t)a[i];
        q[i] = (uint64_t)(cur / base);
        rem  = cur % base;
    }

    return (uint64_t)rem;
}
#endif

static inline void mp_append_dec_u64(std::string &out, uint64_t v) {
    char buf[20];
    int pos = 20;

    do {
        const uint64_t q = v / 10u;
        buf[--pos] = (char)('0' + (v - q * 10u));
        v = q;
    } while (v != 0);

    out.append(buf + pos, 20 - pos);
}

static inline void mp_append_dec_padded_9(std::string &out, uint32_t v) {
    char buf[9];

    for (int i = 8; i >= 0; --i) {
        const uint32_t q = v / 10u;
        buf[i] = (char)('0' + (v - q * 10u));
        v = q;
    }

    out.append(buf, 9);
}

#if defined(__SIZEOF_INT128__)
static inline void mp_append_dec_padded_19(std::string &out, uint64_t v) {
    char buf[19];

    for (int i = 18; i >= 0; --i) {
        const uint64_t q = v / 10u;
        buf[i] = (char)('0' + (v - q * 10u));
        v = q;
    }

    out.append(buf, 19);
}
#endif

static constexpr char MP_DEC_PAIR_TABLE[] =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";

static inline char *mp_write_dec_u64_back(char *p, uint64_t v) {
    while (v >= 100u) {
        const uint64_t q = v / 100u;
        const unsigned r = (unsigned)(v - q * 100u);

        p -= 2;
        p[0] = MP_DEC_PAIR_TABLE[2u * r + 0u];
        p[1] = MP_DEC_PAIR_TABLE[2u * r + 1u];

        v = q;
    }

    if (v >= 10u) {
        const unsigned r = (unsigned)v;

        p -= 2;
        p[0] = MP_DEC_PAIR_TABLE[2u * r + 0u];
        p[1] = MP_DEC_PAIR_TABLE[2u * r + 1u];
    } else {
        *--p = (char)('0' + v);
    }

    return p;
}

static inline char *mp_write_dec_19_back(char *p, uint64_t v) {
    for (int i = 0; i < 9; ++i) {
        const uint64_t q = v / 100u;
        const unsigned r = (unsigned)(v - q * 100u);

        p -= 2;
        p[0] = MP_DEC_PAIR_TABLE[2u * r + 0u];
        p[1] = MP_DEC_PAIR_TABLE[2u * r + 1u];

        v = q;
    }

    *--p = (char)('0' + v);

    return p;
}

static inline char *mp_write_dec_9_back(char *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        const uint32_t q = v / 100u;
        const unsigned r = (unsigned)(v - q * 100u);

        p -= 2;
        p[0] = MP_DEC_PAIR_TABLE[2u * r + 0u];
        p[1] = MP_DEC_PAIR_TABLE[2u * r + 1u];

        v = q;
    }

    *--p = (char)('0' + v);

    return p;
}

static inline void mp_store_hex16(char *out, uint64_t x) {
    static constexpr char hex[] = "0123456789abcdef";

    out[0]  = hex[(x >> 60) & 0xFu];
    out[1]  = hex[(x >> 56) & 0xFu];
    out[2]  = hex[(x >> 52) & 0xFu];
    out[3]  = hex[(x >> 48) & 0xFu];
    out[4]  = hex[(x >> 44) & 0xFu];
    out[5]  = hex[(x >> 40) & 0xFu];
    out[6]  = hex[(x >> 36) & 0xFu];
    out[7]  = hex[(x >> 32) & 0xFu];
    out[8]  = hex[(x >> 28) & 0xFu];
    out[9]  = hex[(x >> 24) & 0xFu];
    out[10] = hex[(x >> 20) & 0xFu];
    out[11] = hex[(x >> 16) & 0xFu];
    out[12] = hex[(x >> 12) & 0xFu];
    out[13] = hex[(x >> 8)  & 0xFu];
    out[14] = hex[(x >> 4)  & 0xFu];
    out[15] = hex[x & 0xFu];
}

std::string mp_get_str(const uint64_t *a, uint32_t base) {
    if (base == 0u) base = 10u;
    if (base < 2u || base > 16u) return {};

    if (mp_is_zero(a)) return {"0"};

    if (base == 16u) {
#if MP_N64 == 4
        char buf[64];

        mp_store_hex16(buf + 0,  a[3]);
        mp_store_hex16(buf + 16, a[2]);
        mp_store_hex16(buf + 32, a[1]);
        mp_store_hex16(buf + 48, a[0]);

        size_t first = 0;
        while (first < 63 && buf[first] == '0') {
            ++first;
        }

        return std::string(buf + first, 64u - first);
    #else
        static constexpr char hex[] = "0123456789abcdef";

        int top = MP_N64 - 1;
        while (top > 0 && a[top] == 0) {
            --top;
        }

        uint64_t x = a[top];

        int shift = 60;
        while (shift > 0 && ((x >> shift) & 0xFu) == 0) {
            shift -= 4;
        }

        std::string out;
        out.reserve((size_t)top * 16u + (size_t)(shift / 4 + 1));

        for (; shift >= 0; shift -= 4) {
            out.push_back(hex[(x >> shift) & 0xFu]);
        }

        for (int i = top - 1; i >= 0; --i) {
            x = a[i];

            out.push_back(hex[(x >> 60) & 0xFu]);
            out.push_back(hex[(x >> 56) & 0xFu]);
            out.push_back(hex[(x >> 52) & 0xFu]);
            out.push_back(hex[(x >> 48) & 0xFu]);
            out.push_back(hex[(x >> 44) & 0xFu]);
            out.push_back(hex[(x >> 40) & 0xFu]);
            out.push_back(hex[(x >> 36) & 0xFu]);
            out.push_back(hex[(x >> 32) & 0xFu]);
            out.push_back(hex[(x >> 28) & 0xFu]);
            out.push_back(hex[(x >> 24) & 0xFu]);
            out.push_back(hex[(x >> 20) & 0xFu]);
            out.push_back(hex[(x >> 16) & 0xFu]);
            out.push_back(hex[(x >> 12) & 0xFu]);
            out.push_back(hex[(x >> 8)  & 0xFu]);
            out.push_back(hex[(x >> 4)  & 0xFu]);
            out.push_back(hex[x & 0xFu]);
        }
        return out;
#endif
    }

    uint64_t v[MP_N64];
    mp_copy(v, a);

    if (base == 10u) {
#if defined(__SIZEOF_INT128__) && MP_N64 == 4
        static constexpr uint64_t DEC_BASE = 10000000000000000000ULL; // 1e19

        uint64_t q0[MP_N64];
        uint64_t q1[MP_N64];
        uint64_t q2[MP_N64];
        uint64_t q3[MP_N64];
        uint64_t q4[MP_N64];

        uint64_t chunks[5];
        size_t n_chunks = 0;

        chunks[n_chunks++] = mp_div_u64_base(q0, v, DEC_BASE);

        if (!mp_is_zero(q0)) {
            chunks[n_chunks++] = mp_div_u64_base(q1, q0, DEC_BASE);

            if (!mp_is_zero(q1)) {
                chunks[n_chunks++] = mp_div_u64_base(q2, q1, DEC_BASE);

                if (!mp_is_zero(q2)) {
                    chunks[n_chunks++] = mp_div_u64_base(q3, q2, DEC_BASE);

                    if (!mp_is_zero(q3)) {
                        chunks[n_chunks++] = mp_div_u64_base(q4, q3, DEC_BASE);
                    }
                }
            }
        }

        char buf[(size_t)MP_N64 * 20u + 1u];
        char *end = buf + sizeof(buf);
        char *p = end;

        for (size_t i = 0; i + 1 < n_chunks; ++i) {
            p = mp_write_dec_19_back(p, chunks[i]);
        }

        p = mp_write_dec_u64_back(p, chunks[n_chunks - 1]);

        return std::string(p, (size_t)(end - p));
#elif defined(__SIZEOF_INT128__)
        static constexpr uint64_t DEC_BASE = 10000000000000000000ULL; // 1e19

        uint64_t chunks[(size_t)MP_N64 * 4u];
        size_t n_chunks = 0;

        while (!mp_is_zero(v)) {
            uint64_t q[MP_N64];
            const uint64_t rem = mp_div_u64_base(q, v, DEC_BASE);

            chunks[n_chunks++] = rem;
            mp_copy(v, q);
        }

        char buf[(size_t)MP_N64 * 20u + 1u];
        char *end = buf + sizeof(buf);
        char *p = end;

        for (size_t i = 0; i + 1 < n_chunks; ++i) {
            p = mp_write_dec_19_back(p, chunks[i]);
        }

        p = mp_write_dec_u64_back(p, chunks[n_chunks - 1]);

        return std::string(p, (size_t)(end - p));
#else
    static constexpr uint32_t DEC_BASE = 1000000000u; // 1e9

    uint32_t chunks[(size_t)MP_N64 * 8u];
    size_t n_chunks = 0;

    while (!mp_is_zero(v)) {
        uint64_t q[MP_N64];
        const uint32_t rem = mp_div(q, v, DEC_BASE);

        chunks[n_chunks++] = rem;
        mp_copy(v, q);
    }

    char buf[(size_t)MP_N64 * 20u + 1u];
    char *end = buf + sizeof(buf);
    char *p = end;

    for (size_t i = 0; i + 1 < n_chunks; ++i) {
        p = mp_write_dec_9_back(p, chunks[i]);
    }

    p = mp_write_dec_u64_back(p, chunks[n_chunks - 1]);

    return std::string(p, (size_t)(end - p));
#endif
}

    std::string out;

    while (!mp_is_zero(v)) {
        uint64_t q[MP_N64];
        uint32_t rem = mp_div(q, v, base);

        char digit = (rem < 10u)
            ? (char)('0' + rem)
            : (char)('a' + (rem - 10u));

        out.push_back(digit);
        mp_copy(v, q);
    }

    std::reverse(out.begin(), out.end());
    return out;
}

bool mp_fits_int32(const uint64_t *a) {
    for (int i = 1; i < MP_N64; ++i) {
        if (a[i] != 0) return false;
    }
    return a[0] <= (uint64_t)INT32_MAX;
}

int32_t mp_get_int32(const mp_uint_t a) {
    return (int32_t)a[0];
}

static inline void mp_add_mod(uint64_t *r, const uint64_t *a, const uint64_t *b, const uint64_t *mod) {
    uint64_t carry = mp_add(r, a, b);

    if (carry || mp_cmp(r, mod) >= 0) {
        mp_sub(r, r, mod);
    }
}

static inline void mp_mul_mod(uint64_t *r, const uint64_t *a, uint32_t b, const uint64_t *mod) {
    uint64_t res[MP_N64]; mp_zero(res);

    uint64_t cur[MP_N64];
    if (mp_cmp(a, mod) >= 0) {
        mp_uint_t q, rem;
        mp_div(q, rem, a, mod);
        mp_copy(cur, rem);
    } else {
        mp_copy(cur, a);
    }

    uint32_t k = b;
    while (k) {
        if (k & 1u) {
            uint64_t tmp[MP_N64];
            mp_add_mod(tmp, res, cur, mod);
            mp_copy(res, tmp);
        }
        k >>= 1u;
        if (k) {
            uint64_t tmp[MP_N64];
            mp_add_mod(tmp, cur, cur, mod);
            mp_copy(cur, tmp);
        }
    }
    mp_copy(r, res);
}

bool mp_set_mod(uint64_t *r, const char *str, uint32_t base, const uint64_t *mod) {
    mp_zero(r);

    if (base == 0u) base = 10u;
    if (base < 2u || base > 16u) return false;

    if (base == 16u) {
        uint64_t tmp[MP_N64];
        bool neg = false;

        MpHexParseStatus st = mp_parse_hex_u256(tmp, str, true, &neg);

        if (st == MpHexParseStatus::Invalid) {
            return false;
        }

        if (st == MpHexParseStatus::Ok) {
            if (mp_cmp(tmp, mod) >= 0) {
                uint64_t q[MP_N64];
                uint64_t rem[MP_N64];

                mp_div(q, rem, tmp, mod);
                mp_copy(tmp, rem);
            }

            if (neg && !mp_is_zero(tmp)) {
                uint64_t reduced_neg[MP_N64];

                mp_sub(reduced_neg, mod, tmp);
                mp_copy(tmp, reduced_neg);
            }

            mp_copy(r, tmp);
            return true;
        }
    }

    while (*str && std::isspace((unsigned char)*str)) str++;

    bool neg = false;
    if (*str == '+') str++;
    else if (*str == '-') { neg = true; str++; }

    uint64_t acc[MP_N64];
    mp_zero(acc);

    bool any = false;

    auto reduce_small = [&](uint64_t *x) {
        // x < mod*base + 15  => at most base subtractions (base <= 16)
        while (mp_cmp(x, mod) >= 0) {
            mp_sub(x, x, mod);
        }
    };

    for (; *str; str++) {
        if (std::isspace((unsigned char)*str)) break;

        const char uc = *str;
        uint32_t d;
        if (uc >= '0' && uc <= '9') {
            d = uc - '0';
        } else if (uc >= 'a' && uc <= 'f') {
            d = (uc - 'a') + 10u;
        } else if (uc >= 'A' && uc <= 'F') {
            d = (uc - 'A') + 10u;
        } else {
            return false;
        }
        if (d >= base) return false;

#if !defined(__SIZEOF_INT128__)
        {
            uint64_t t1[MP_N64];
            mp_mul_mod(t1, acc, base, mod);
            uint64_t dv[MP_N64];
            mp_set(dv, d);
            uint64_t t2[MP_N64];
            mp_add_mod(t2, t1, dv, mod);
            mp_copy(acc, t2);
            any = true;
            continue;
        }
#else

        uint64_t tmp[MP_N64];
        {
            __uint128_t carry = 0;
            for (size_t i = 0; i < MP_N64; i++) {
                __uint128_t prod = (__uint128_t)acc[i] * (__uint128_t)base + carry;
                tmp[i] = (uint64_t)prod;
                carry = prod >> 2*MP_N;
            }

            if (carry) {
                uint64_t t1[MP_N64];
                mp_mul_mod(t1, acc, base, mod);
                uint64_t dv[MP_N64];
                mp_set(dv, d);
                uint64_t t2[MP_N64];
                mp_add_mod(t2, t1, dv, mod);
                mp_copy(acc, t2);
                any = true;
                continue;
            }
        }

        // tmp += d
        {
            uint64_t c = d;

            for (size_t i = 0; i < MP_N64 && c; i++) {
                uint64_t before = tmp[i];
                tmp[i] += c;
                c = tmp[i] < before ? 1u : 0u;
            }

            if (c) {
                uint64_t t1[MP_N64];
                mp_mul_mod(t1, acc, base, mod);
                uint64_t dv[MP_N64];
                mp_set(dv, d);
                uint64_t t2[MP_N64];
                mp_add_mod(t2, t1, dv, mod);
                mp_copy(acc, t2);
                any = true;
                continue;
            }
        }

        mp_copy(acc, tmp);
        reduce_small(acc);
        any = true;
#endif
    }

    if (!any) return false;

    while (*str && std::isspace((unsigned char)*str)) str++;
    if (*str != '\0') return false;

    if (neg && !mp_is_zero(acc)) {
        uint64_t tmp[MP_N64];
        mp_sub(tmp, mod, acc);
        mp_copy(acc, tmp);
    }

    mp_copy(r, acc);
    return true;
}

static inline unsigned mp_clz64(uint64_t x) { return x ? __builtin_clzll(x) : 64; }

static inline void mp_mul64(uint64_t a, uint64_t b, uint64_t &lo, uint64_t &hi) {
    const uint64_t a0 = (uint32_t)a;
    const uint64_t a1 = a >> MP_N;
    const uint64_t b0 = (uint32_t)b;
    const uint64_t b1 = b >> MP_N;

    const uint64_t p00 = a0 * b0;
    const uint64_t p01 = a0 * b1;
    const uint64_t p10 = a1 * b0;
    const uint64_t p11 = a1 * b1;

    const uint64_t mid = (p00 >> MP_N) + (uint32_t)p01 + (uint32_t)p10;
    lo = (p00 & 0xFFFFFFFFULL) | (mid << MP_N);
    hi = p11 + (p01 >> MP_N) + (p10 >> MP_N) + (mid >> MP_N);
}

static inline int mp_num_limbs(const uint64_t *a) {
    for (int i = MP_N64 - 1; i >= 0; --i) {
        if (a[i] != 0) return i + 1;
    }

    return 0;
}

static inline uint64_t div_2by1(uint64_t *q, uint64_t hi, uint64_t lo, uint64_t v) {
    uint64_t qq  = 0;
    uint64_t rem = hi;

    for (int bit = (2 * MP_N) - 1; bit >= 0; --bit) {
        // Shift remainder left by 1 and bring next bit from lo.
        uint64_t overflow = rem >> ((2 * MP_N) - 1);
        rem = (rem << 1) | ((lo >> bit) & 1ULL);

        // If previous top bit was 1, actual shifted remainder was >= 2^64.
        // Since rem_before < v, shifted remainder < 2*v + 1, so one subtraction is enough.
        if (overflow || rem >= v) {
            rem -= v;
            qq |= (1ULL << bit);
        }
    }

    *q = qq;
    return rem;
}

static inline uint64_t div_1word(uint64_t *q, const uint64_t *u, uint64_t v)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t rem = 0;

    for (int i = MP_N64 - 1; i >= 0; i--) {
        __uint128_t cur = rem << 2*MP_N | (__uint128_t)u[i];
        q[i] = (uint64_t)(cur / v);
        rem  = (uint64_t)(cur % v);
    }

    return (uint64_t)rem;
#else
uint64_t rem = 0;

for (int i = MP_N64 - 1; i >= 0; --i) {
    uint64_t qi;
    rem = div_2by1(&qi, rem, u[i], v);
    q[i] = qi;
}

return rem;
#endif
}

static inline uint64_t mul_sub_knuth(uint64_t *u, const uint64_t *v, int n, uint64_t qhat)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t carry = 0;
    uint64_t borrow = 0;

    for (int i = 0; i < n; i++) {
        __uint128_t prod = (__uint128_t)qhat * (__uint128_t)v[i] + carry;
        uint64_t pl = (uint64_t)prod;
        carry = prod >> 2*MP_N;
        __int128_t t = (__int128_t)u[i] - (__int128_t)pl - (__int128_t)borrow;
        u[i] = (uint64_t)t;
        borrow = t < 0 ? 1u : 0u;
    }

    __int128_t ttop = (__int128_t)u[n] - (__int128_t)carry - (__int128_t)borrow;
    u[n] = (uint64_t)ttop;

    return ttop < 0 ? 1u : 0u;
#else
    uint64_t carry  = 0;
    uint64_t borrow = 0;

    for (int i = 0; i < n; ++i) {
        uint64_t hi, lo;
        mp_mul64(qhat, v[i], lo, hi);

        // Add previous carry into low part of the product.
        uint64_t c = add_carry(&lo, lo, carry, 0);
        hi += c;

        // Subtract low limb and incoming borrow from u[i].
        borrow = sub_borrow(&u[i], u[i], lo, borrow);

        // High limb becomes carry for next step.
        carry = hi;
    }

    // Subtract final carry and borrow from top limb u[n].
    return sub_borrow(&u[n], u[n], carry, borrow);
#endif
}

static inline uint64_t add_back_knuth(uint64_t *u, const uint64_t *v, int n)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t carry = 0;

    for (int i = 0; i < n; i++) {
        __uint128_t t = (__uint128_t)u[i] + (__uint128_t)v[i] + carry;
        u[i] = (uint64_t)t;
        carry = t >> 2*MP_N;
    }

    __uint128_t ttop = (__uint128_t)u[n] + carry;
    u[n] = (uint64_t)ttop;

    return (uint64_t)(ttop >> 2*MP_N);
#else
    uint64_t carry = 0;

    for (int i = 0; i < n; ++i) {
        carry = add_carry(&u[i], u[i], v[i], carry);
    }

    return add_carry(&u[n], u[n], 0, carry);
#endif
}

void mp_div(uint64_t *q, uint64_t *r, const uint64_t *num, const uint64_t *den) {
    if (mp_is_zero(den)) {
        mp_zero(q);
        mp_zero(r);
        return;
    }

    if (mp_cmp(num, den) < 0) {
        mp_zero(q);
        mp_copy(r, num);
        return;
    }

#if !defined(__SIZEOF_INT128__)
    int m = mp_num_limbs(num);
    int n = mp_num_limbs(den);

    if (n == 1) {
        uint64_t v = den[0];
        mp_uint_t u;
        mp_uint_t qq = {0};

        mp_copy(u, num);

        uint64_t rem = div_1word(qq, u, v);

        mp_copy(q, qq);
        mp_set(r, rem);
        return;
    }

    mp_uint_t vnorm = {0};
    uint64_t  unorm[MP_N64 + 1] = {0};
    mp_uint_t vraw;
    uint64_t  uraw[MP_N64 + 1] = {0};

    mp_copy(vraw, den);
    for (int i = 0; i < MP_N64; ++i) {
        uraw[i] = num[i];
    }
    uraw[MP_N64] = 0;

    unsigned s = mp_clz64(vraw[n - 1]);

    if (s == 0) {
        for (int i = 0; i < n; ++i) vnorm[i] = vraw[i];
        for (int i = n; i < MP_N64; ++i) vnorm[i] = 0;
        for (int i = 0; i < MP_N64 + 1; ++i) unorm[i] = uraw[i];
    } else {
        uint64_t carry = 0;
        for (int i = 0; i < n; ++i) {
            uint64_t x = vraw[i];
            vnorm[i] = (x << s) | carry;
            carry = x >> ((2 * MP_N) - s);
        }

        for (int i = n; i < MP_N64; ++i) vnorm[i] = 0;

        carry = 0;
        for (int i = 0; i < MP_N64 + 1; ++i) {
            uint64_t x = uraw[i];
            unorm[i] = (x << s) | carry;
            carry = x >> ((2 * MP_N) - s);
        }
    }

    int qn = m - n + 1;
    mp_uint_t qlimb = {0};

    const uint64_t v1 = vnorm[n - 1];
    const uint64_t v2 = vnorm[n - 2];

    for (int j = qn - 1; j >= 0; --j) {
        uint64_t qhat, rhat;
        rhat = div_2by1(&qhat, unorm[j + n], unorm[j + n - 1], v1);

        for (;;) {
            uint64_t lo, hi;
            mp_mul64(qhat, v2, lo, hi);

            bool too_big = false;
            if (hi > rhat) {
                too_big = true;
            } else if (hi == rhat && lo > unorm[j + n - 2]) {
                too_big = true;
            }

            if (!too_big) break;

            --qhat;
            uint64_t old_rhat = rhat;
            rhat += v1;
            if (rhat < old_rhat) break;
        }

        uint64_t *u_seg = &unorm[j];
        uint64_t borrow_out = mul_sub_knuth(u_seg, vnorm, n, qhat);

        if (borrow_out) {
            add_back_knuth(u_seg, vnorm, n);
            --qhat;
        }

        qlimb[j] = qhat;
    }

    mp_uint_t rlimb = {0};

    if (s == 0) {
        for (int i = 0; i < n; ++i) rlimb[i] = unorm[i];
    } else {
        uint64_t carry = 0;
        for (int i = n - 1; i >= 0; --i) {
            uint64_t x = unorm[i];
            rlimb[i] = (x >> s) | carry;
            carry = x << ((2 * MP_N) - s);
        }
    }

    mp_copy(q, qlimb);
    mp_copy(r, rlimb);
    return;
#else
    int m = mp_num_limbs(num);
    int n = mp_num_limbs(den);

    if (n == 1) {
        uint64_t v = den[0];
        mp_uint_t u;
        mp_uint_t qq = {};

        mp_copy(u, num);

        uint64_t rem = div_1word(qq, u, v);

        mp_copy(q, qq);
        mp_set(r, rem);
        return;
    }

    mp_uint_t vnorm = {};
    uint64_t  unorm[MP_N64 + 1] = {};
    mp_uint_t vraw;
    uint64_t  uraw[MP_N64 + 1] = {};

    mp_copy(vraw, den);
    for (int i = 0; i < MP_N64; ++i) {
        uraw[i] = num[i];
    }
    uraw[MP_N64] = 0;

    unsigned s = mp_clz64(vraw[n - 1]);

    if (s == 0) {
        for (int i = 0; i < n; i++) vnorm[i] = vraw[i];
        for (int i = n; i < MP_N64; i++) vnorm[i] = 0;
        for (int i = 0; i < MP_N64 + 1; i++) unorm[i] = uraw[i];
    } else {
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            uint64_t x = vraw[i];
            vnorm[i] = x << s | carry;
            carry = x >> (2*MP_N - s);
        }

        for (int i = n; i < MP_N64; i++) vnorm[i] = 0;

        carry = 0;

        for (int i = 0; i < MP_N64 + 1; i++) {
            uint64_t x = uraw[i];
            unorm[i] = x << s | carry;
            carry = x >> (2*MP_N - s);
        }
    }

    int qn = m - n + 1;
    mp_uint_t qlimb = {};

    const uint64_t v1 = vnorm[n - 1];
    const uint64_t v2 = vnorm[n - 2];

    for (int j = qn - 1; j >= 0; j--) {
        uint64_t qhat;
        __uint128_t rhat;

        if (unorm[j + n] == v1) {
            qhat = UINT64_MAX;
            rhat = (__uint128_t)unorm[j + n - 1] + (__uint128_t)v1;
        } else {
            __uint128_t uj2 =
                (__uint128_t)unorm[j + n] << (2*MP_N) |
                (__uint128_t)unorm[j + n - 1];

            qhat = (uint64_t)(uj2 / v1);
            rhat = uj2 % v1;
        }

        for (;;) {
            __uint128_t left  = (__uint128_t)qhat * (__uint128_t)v2;
            __uint128_t right = (rhat << 2*MP_N) | (__uint128_t)unorm[j + n - 2];
            if (left <= right) break;

            --qhat;
            rhat += (__uint128_t)v1;
            if (rhat >= (((__uint128_t)1) << 2*MP_N)) break;
        }

        uint64_t *u_seg = &unorm[j];
        uint64_t borrow_out = mul_sub_knuth(u_seg, vnorm, n, qhat);

        if (borrow_out) {
            add_back_knuth(u_seg, vnorm, n);
            qhat--;
        }

        qlimb[j] = qhat;
    }

    mp_uint_t rlimb = {};

    if (s == 0) {
        for (int i = 0; i < n; i++) rlimb[i] = unorm[i];
    } else {
        uint64_t carry = 0;

        for (int i = n - 1; i >= 0; i--) {
            uint64_t x = unorm[i];
            rlimb[i] = x >> s | carry;
            carry = x << (2*MP_N - s);
        }
    }

    mp_copy(q, qlimb);
    mp_copy(r, rlimb);
#endif
}

static inline void mp_mul_full(uint64_t *out2n, const uint64_t *a, const uint64_t *b)
{
#if defined(__SIZEOF_INT128__)
    for (int i = 0; i < 2 * MP_N64; ++i) {
        out2n[i] = 0;
    }

    for (int i = 0; i < MP_N64; ++i) {
        __uint128_t carry = 0;
        for (int j = 0; j < MP_N64; ++j) {
            __uint128_t cur = (__uint128_t)a[i] * (__uint128_t)b[j]
                            + (__uint128_t)out2n[i + j]
                            + carry;
            out2n[i + j] = (uint64_t)cur;
            carry = cur >> (2 * MP_N);
        }

        int k = i + MP_N64;
        while (carry) {
            __uint128_t cur = (__uint128_t)out2n[k] + carry;
            out2n[k] = (uint64_t)cur;
            carry = cur >> (2 * MP_N);
            ++k;
        }
    }
#else
    for (int i = 0; i < 2 * MP_N64; ++i) out2n[i] = 0;

    for (int i = 0; i < MP_N64; ++i) {
        uint64_t carry = 0;

        for (int j = 0; j < MP_N64; ++j) {
            uint64_t lo, hi;
            mp_mul64(a[i], b[j], lo, hi);

            uint64_t x;
            uint64_t c0 = add_carry(&x, out2n[i + j], lo, 0);
            uint64_t c1 = add_carry(&x, x, carry, 0);
            out2n[i + j] = x;

            carry = hi + c0 + c1;
        }

        int k = i + MP_N64;
        while (carry && k < 2 * MP_N64) {
            uint64_t x;
            uint64_t c = add_carry(&x, out2n[k], carry, 0);
            out2n[k] = x;
            carry = c;
            ++k;
        }
    }
#endif
}

static inline void mp_mod_2n_n(uint64_t *r, const uint64_t *num2n, const uint64_t *den)
{
    const int n = mp_num_limbs(den);

    if (n == 0) {
        mp_zero(r);
        return;
    }

    if (n == 1) {
        uint64_t rem = 0;
#if defined(__SIZEOF_INT128__)
        for (int i = 2 * MP_N64 - 1; i >= 0; --i) {
            __uint128_t cur = ((__uint128_t)rem << (2 * MP_N)) | (__uint128_t)num2n[i];
            rem = (uint64_t)(cur % den[0]);
        }
#else
        for (int i = 2 * MP_N64 - 1; i >= 0; --i) {
            uint64_t qdummy;
            rem = div_2by1(&qdummy, rem, num2n[i], den[0]);
        }
#endif
        mp_set(r, rem);
        return;
    }

    uint64_t vnorm[MP_N64] = {0};
    uint64_t unorm[2 * MP_N64 + 1] = {0};

    const unsigned s = mp_clz64(den[n - 1]);

    if (s == 0) {
        for (int i = 0; i < n; ++i) {
            vnorm[i] = den[i];
        }
        for (int i = 0; i < 2 * MP_N64; ++i) {
            unorm[i] = num2n[i];
        }
        unorm[2 * MP_N64] = 0;
    } else {
        uint64_t carry = 0;
        for (int i = 0; i < n; ++i) {
            uint64_t x = den[i];
            vnorm[i] = (x << s) | carry;
            carry = x >> ((2 * MP_N) - s);
        }

        carry = 0;
        for (int i = 0; i < 2 * MP_N64; ++i) {
            uint64_t x = num2n[i];
            unorm[i] = (x << s) | carry;
            carry = x >> ((2 * MP_N) - s);
        }
        unorm[2 * MP_N64] = carry;
    }

    const uint64_t v1 = vnorm[n - 1];
    const uint64_t v2 = vnorm[n - 2];

    const int qn = 2 * MP_N64 - n + 1;

    for (int j = qn - 1; j >= 0; --j) {
        uint64_t qhat, rhat;

#if defined(__SIZEOF_INT128__)
        __uint128_t uj2 =
            ((__uint128_t)unorm[j + n] << (2 * MP_N)) |
            (__uint128_t)unorm[j + n - 1];

        qhat = (uint64_t)(uj2 / v1);
        rhat = (uint64_t)(uj2 % v1);

        for (;;) {
            __uint128_t left =
                (__uint128_t)qhat * (__uint128_t)v2;
            __uint128_t right =
                ((__uint128_t)rhat << (2 * MP_N)) |
                (__uint128_t)unorm[j + n - 2];

            if (left <= right) break;

            --qhat;
            uint64_t old_rhat = rhat;
            rhat += v1;
            if (rhat < old_rhat) break;
        }
#else
        rhat = div_2by1(&qhat, unorm[j + n], unorm[j + n - 1], v1);

        for (;;) {
            uint64_t lo, hi;
            mp_mul64(qhat, v2, lo, hi);

            bool too_big = false;
            if (hi > rhat) {
                too_big = true;
            } else if (hi == rhat && lo > unorm[j + n - 2]) {
                too_big = true;
            }

            if (!too_big) break;

            --qhat;
            uint64_t old_rhat = rhat;
            rhat += v1;
            if (rhat < old_rhat) break;
        }
#endif

        uint64_t borrow_out = mul_sub_knuth(&unorm[j], vnorm, n, qhat);
        if (borrow_out) {
            add_back_knuth(&unorm[j], vnorm, n);
        }
    }

    mp_zero(r);

    if (s == 0) {
        for (int i = 0; i < n; ++i) {
            r[i] = unorm[i];
        }
    } else {
        uint64_t carry = 0;
        for (int i = n - 1; i >= 0; --i) {
            uint64_t x = unorm[i];
            r[i] = (x >> s) | carry;
            carry = x << ((2 * MP_N) - s);
        }
    }

    while (mp_cmp(r, den) >= 0) {
        mp_sub(r, r, den);
    }
}


static inline void mp_mulmod(uint64_t *r, const uint64_t *a, const uint64_t *b, const uint64_t *mod)
{
    if (mp_is_zero(mod)) {
        mp_zero(r);
        return;
    }

    uint64_t prod[2 * MP_N64];
    mp_mul_full(prod, a, b);
    mp_mod_2n_n(r, prod, mod);
}

#if MP_N64 == 4 && defined(__SIZEOF_INT128__)

static inline uint64_t mp_mont_n0inv_4(uint64_t n0) {
    // Computes -n0^{-1} mod 2^64.
    // Valid only when n0 is odd.
    uint64_t x = 1;

    x *= 2ULL - n0 * x;
    x *= 2ULL - n0 * x;
    x *= 2ULL - n0 * x;
    x *= 2ULL - n0 * x;
    x *= 2ULL - n0 * x;
    x *= 2ULL - n0 * x;

    return (uint64_t)0 - x;
}

static inline void mp_mont_reduce_4(
    uint64_t *r,
    const uint64_t *in,
    const uint64_t *mod,
    uint64_t n0inv
) {
    uint64_t t[9] = {
        in[0], in[1], in[2], in[3],
        in[4], in[5], in[6], in[7],
        0
    };

    for (int i = 0; i < 4; ++i) {
        const uint64_t m = t[i] * n0inv;

        __uint128_t z;
        __uint128_t carry = 0;

        z = (__uint128_t)m * mod[0] + t[i + 0] + carry;
        t[i + 0] = (uint64_t)z;
        carry = z >> 64;

        z = (__uint128_t)m * mod[1] + t[i + 1] + carry;
        t[i + 1] = (uint64_t)z;
        carry = z >> 64;

        z = (__uint128_t)m * mod[2] + t[i + 2] + carry;
        t[i + 2] = (uint64_t)z;
        carry = z >> 64;

        z = (__uint128_t)m * mod[3] + t[i + 3] + carry;
        t[i + 3] = (uint64_t)z;
        carry = z >> 64;

        z = (__uint128_t)t[i + 4] + carry;
        t[i + 4] = (uint64_t)z;

        uint64_t c = (uint64_t)(z >> 64);

        for (int k = i + 5; c && k < 9; ++k) {
            z = (__uint128_t)t[k] + c;
            t[k] = (uint64_t)z;
            c = (uint64_t)(z >> 64);
        }
    }

    r[0] = t[4];
    r[1] = t[5];
    r[2] = t[6];
    r[3] = t[7];

    if (t[8] != 0 || mp_cmp(r, mod) >= 0) {
        mp_sub(r, r, mod);
    }
}

static inline void mp_mont_mul_4(
    uint64_t *r,
    const uint64_t *a,
    const uint64_t *b,
    const uint64_t *mod,
    uint64_t n0inv
) {
    uint64_t prod[8];

    mp_mul_full(prod, a, b);
    mp_mont_reduce_4(r, prod, mod, n0inv);
}

static inline void mp_mont_compute_r_r2_4(
    uint64_t *r_mod,
    uint64_t *r2_mod,
    const uint64_t *mod
) {
    // R = 2^256.
    // r_mod = R mod mod.
    uint64_t r_full[8] = {
        0, 0, 0, 0,
        1, 0, 0, 0
    };

    mp_mod_2n_n(r_mod, r_full, mod);

    // r2_mod = R^2 mod mod = (R mod mod)^2 mod mod.
    uint64_t prod[8];
    mp_mul_full(prod, r_mod, r_mod);
    mp_mod_2n_n(r2_mod, prod, mod);
}

static inline void mp_to_mont_4(
    uint64_t *r,
    const uint64_t *a,
    const uint64_t *r2_mod,
    const uint64_t *mod,
    uint64_t n0inv
) {
    // aM = a * R mod mod = REDC(a * R^2)
    mp_mont_mul_4(r, a, r2_mod, mod, n0inv);
}

static inline void mp_from_mont_4(
    uint64_t *r,
    const uint64_t *a_mont,
    const uint64_t *mod,
    uint64_t n0inv
) {
    uint64_t one[4] = {1, 0, 0, 0};

    // a = REDC(aM * 1)
    mp_mont_mul_4(r, a_mont, one, mod, n0inv);
}

struct MpMontCtx4 {
    uint64_t mod[4];
    uint64_t r_mod[4];
    uint64_t r2_mod[4];
    uint64_t n0inv;
};

static constexpr MpMontCtx4 MP_MONT_BN254_FQ = {
    {
        0x3c208c16d87cfd47ULL,
        0x97816a916871ca8dULL,
        0xb85045b68181585dULL,
        0x30644e72e131a029ULL
    },
    {
        0xd35d438dc58f0d9dULL,
        0x0a78eb28f5c70b3dULL,
        0x666ea36f7879462cULL,
        0x0e0a77c19a07df2fULL
    },
    {
        0xf32cfc5b538afa89ULL,
        0xb5e71911d44501fbULL,
        0x47ab1eff0a417ff6ULL,
        0x06d89f71cab8351fULL
    },
    0x87d20782e4866389ULL
};

static constexpr MpMontCtx4 MP_MONT_BN254_FR = {
    {
        0x43e1f593f0000001ULL,
        0x2833e84879b97091ULL,
        0xb85045b68181585dULL,
        0x30644e72e131a029ULL
    },
    {
        0xac96341c4ffffffbULL,
        0x36fc76959f60cd29ULL,
        0x666ea36f7879462eULL,
        0x0e0a77c19a07df2fULL
    },
    {
        0x1bb8e645ae216da7ULL,
        0x53fe3ab1e35c59e3ULL,
        0x8c49833d53bb8085ULL,
        0x0216d0b17f4e44a5ULL
    },
    0xc2e1f593efffffffULL
};

static inline bool mp_mont_mod_eq_4(const uint64_t *a, const uint64_t *b) {
    return ((a[0] ^ b[0]) |
            (a[1] ^ b[1]) |
            (a[2] ^ b[2]) |
            (a[3] ^ b[3])) == 0;
}

static inline const MpMontCtx4 *mp_mont_get_known_ctx_4(const uint64_t *mod) {
    if (mp_mont_mod_eq_4(mod, MP_MONT_BN254_FQ.mod)) {
        return &MP_MONT_BN254_FQ;
    }

    if (mp_mont_mod_eq_4(mod, MP_MONT_BN254_FR.mod)) {
        return &MP_MONT_BN254_FR;
    }

    return nullptr;
}


static inline int mp_exp_popcount_4(const uint64_t *exp) {
    return __builtin_popcountll(exp[0])
         + __builtin_popcountll(exp[1])
         + __builtin_popcountll(exp[2])
         + __builtin_popcountll(exp[3]);
}

static inline void mp_pow_mod_mont_windowed_4(
    uint64_t *r,
    const uint64_t *base,
    const uint64_t *exp,
    const uint64_t *mod,
    uint64_t n0inv,
    const uint64_t *r2_mod,
    int topBit,
    int windowBits
) {
    // Supports windowBits 4 or 5. Table stores odd powers: 1,3,...,(2^w - 1).
    uint64_t table[16][4];
    const int tableSize = 1 << (windowBits - 1);

    // table[0] = base^1 in Montgomery form.
    mp_to_mont_4(table[0], base, r2_mod, mod, n0inv);

    // b2 = base^2 in Montgomery form.
    uint64_t b2[4];
    mp_mont_mul_4(b2, table[0], table[0], mod, n0inv);

    // table[i] = base^(2*i + 1) in Montgomery form.
    for (int i = 1; i < tableSize; ++i) {
        mp_mont_mul_4(table[i], table[i - 1], b2, mod, n0inv);
    }

    uint64_t one[4];
    mp_set(one, 1u);

    uint64_t acc[4];
    mp_to_mont_4(acc, one, r2_mod, mod, n0inv);

    for (int i = topBit; i >= 0;) {
        const int limb = i / (2 * MP_N);
        const int bit  = i % (2 * MP_N);

        if (((exp[limb] >> bit) & 1ULL) == 0) {
            uint64_t sq[4];
            mp_mont_mul_4(sq, acc, acc, mod, n0inv);
            mp_copy(acc, sq);
            --i;
            continue;
        }

        int width = std::min(windowBits, i + 1);
        int low = i - width + 1;

        // Keep the selected window odd by moving its low end to a set bit.
        while (low < i) {
            const int low_limb = low / (2 * MP_N);
            const int low_bit  = low % (2 * MP_N);

            if ((exp[low_limb] >> low_bit) & 1ULL) {
                break;
            }

            ++low;
            --width;
        }

        unsigned window = 0;
        for (int j = i; j >= low; --j) {
            const int j_limb = j / (2 * MP_N);
            const int j_bit  = j % (2 * MP_N);
            window = (window << 1) | (unsigned)((exp[j_limb] >> j_bit) & 1ULL);
        }

        for (int j = 0; j < width; ++j) {
            uint64_t sq[4];
            mp_mont_mul_4(sq, acc, acc, mod, n0inv);
            mp_copy(acc, sq);
        }

        uint64_t tmp[4];
        mp_mont_mul_4(tmp, acc, table[window >> 1], mod, n0inv);
        mp_copy(acc, tmp);

        i = low - 1;
    }

    mp_from_mont_4(r, acc, mod, n0inv);
}

#endif

void mp_pow_mod(uint64_t *r, const uint64_t *base, const uint64_t *exp, const uint64_t *mod)
{
    mp_uint_t one;
    mp_set(one, 1u);

    if (mp_cmp(mod, one) == 0) {
        mp_zero(r);
        return;
    }

    mp_uint_t bcur;

    if (mp_cmp(base, mod) >= 0) {
        mp_uint_t q, rem;
        mp_div(q, rem, base, mod);
        mp_copy(bcur, rem);
    } else {
        mp_copy(bcur, base);
    }

    int topBit = -1;

    for (int limb = MP_N64 - 1; limb >= 0 && topBit < 0; --limb) {
        uint64_t w = exp[limb];

        if (!w) {
            continue;
        }

        for (int bit = 2 * MP_N - 1; bit >= 0; --bit) {
            if ((w >> bit) & 1u) {
                topBit = limb * 2 * MP_N + bit;
                break;
            }
        }
    }

    if (topBit < 0) {
        mp_copy(r, one);
        return;
    }

#if MP_N64 == 4 && defined(__SIZEOF_INT128__)
    if ((mod[0] & 1ULL) != 0) {
        uint64_t n0inv;
        uint64_t r2_mod_local[4];
        const uint64_t *r2_mod_ptr = nullptr;

        const MpMontCtx4 *ctx = mp_mont_get_known_ctx_4(mod);

        if (ctx) {
            n0inv = ctx->n0inv;
            r2_mod_ptr = ctx->r2_mod;
        } else {
            n0inv = mp_mont_n0inv_4(mod[0]);

            uint64_t r_mod_local[4];
            mp_mont_compute_r_r2_4(r_mod_local, r2_mod_local, mod);

            r2_mod_ptr = r2_mod_local;
        }

        const int popcnt = mp_exp_popcount_4(exp);

        if (topBit >= 191 && popcnt >= 64) {
            mp_pow_mod_mont_windowed_4(r, bcur, exp, mod, n0inv, r2_mod_ptr, topBit, 5);
            return;
        }

        if (topBit >= 63 && popcnt >= 16) {
            mp_pow_mod_mont_windowed_4(r, bcur, exp, mod, n0inv, r2_mod_ptr, topBit, 4);
            return;
        }

        uint64_t b_mont[4];
        uint64_t acc_mont[4];

        mp_to_mont_4(b_mont, bcur, r2_mod_ptr, mod, n0inv);
        mp_copy(acc_mont, b_mont);

        for (int i = topBit - 1; i >= 0; --i) {
            uint64_t sq[4];

            mp_mont_mul_4(sq, acc_mont, acc_mont, mod, n0inv);
            mp_copy(acc_mont, sq);

            const int limb = i / (2 * MP_N);
            const int bit  = i % (2 * MP_N);

            if ((exp[limb] >> bit) & 1u) {
                uint64_t tmp[4];

                mp_mont_mul_4(tmp, acc_mont, b_mont, mod, n0inv);
                mp_copy(acc_mont, tmp);
            }
        }

        mp_from_mont_4(r, acc_mont, mod, n0inv);
        return;
    }
#endif

    mp_uint_t acc;
    mp_copy(acc, bcur);

    for (int i = topBit - 1; i >= 0; --i) {
        mp_uint_t sq;

        mp_mulmod(sq, acc, acc, mod);
        mp_copy(acc, sq);

        const int limb = i / (2 * MP_N);
        const int bit  = i % (2 * MP_N);

        if ((exp[limb] >> bit) & 1u) {
            mp_uint_t tmp;

            mp_mulmod(tmp, acc, bcur, mod);
            mp_copy(acc, tmp);
        }
    }

    mp_copy(r, acc);
}

void mp_set_mod(uint64_t *r, int64_t a, const uint64_t *mod) {
    if (a >= 0) {
        mp_set(r, (uint64_t)a);

        if (mp_cmp(r, mod) >= 0) {
            uint64_t q[MP_N64], rem[MP_N64];
            mp_div(q, rem, r, mod);
            mp_copy(r, rem);
        }

        return;
    }

#if defined(__SIZEOF_INT128__)
    uint64_t absv = (uint64_t)(-((__int128_t)a));
#else
    uint64_t absv = (uint64_t)(-(uint64_t)a);
#endif

    uint64_t av[MP_N64];
    mp_set(av, absv);

    if (mp_is_zero(av)) {
        mp_zero(r);
        return;
    }

    if (mp_cmp(av, mod) >= 0) {
        uint64_t q[MP_N64], rem[MP_N64];
        mp_div(q, rem, av, mod);
        mp_copy(av, rem);
    }

    if (mp_is_zero(av)) {
        mp_zero(r);
        return;
    }

    mp_sub(r, mod, av);
}

uint64_t mp_mul(uint64_t *r, const uint64_t *a, uint64_t b) {
#if defined(__SIZEOF_INT128__) && MP_N64 == 4
    __uint128_t t;

    t = (__uint128_t)a[0] * b;
    r[0] = (uint64_t)t;
    t >>= 64;

    t += (__uint128_t)a[1] * b;
    r[1] = (uint64_t)t;
    t >>= 64;

    t += (__uint128_t)a[2] * b;
    r[2] = (uint64_t)t;
    t >>= 64;

    t += (__uint128_t)a[3] * b;
    r[3] = (uint64_t)t;

    return (uint64_t)(t >> 64);

#elif defined(__SIZEOF_INT128__)
    __uint128_t carry = 0;
    for (int i = 0; i < MP_N64; i++) {
        __uint128_t t = (__uint128_t)a[i] * (__uint128_t)b + carry;
        r[i] = (uint64_t)t;
        carry = t >> 64;
    }
    return (uint64_t)carry;
#else
    uint64_t carry = 0;
    for (int i = 0; i < MP_N64; i++) {
        uint64_t lo, hi;
        mp_mul64(a[i], b, lo, hi);

        uint64_t out = lo + carry;
        hi += (out < lo);

        r[i] = out;
        carry = hi;
    }
    return carry;
#endif
}

uint64_t mp_addmul(uint64_t *r, const uint64_t *a, size_t n, uint64_t b)
{
#if defined(__SIZEOF_INT128__)

#if MP_N64 == 4
    // mp_addmul(productX, pRawB, Fq_N64/Fr_N64=4, pRawA[i])
    if (n == 4) {
        const __uint128_t p0 = (__uint128_t)a[0] * b;
        const __uint128_t p1 = (__uint128_t)a[1] * b;
        const __uint128_t p2 = (__uint128_t)a[2] * b;
        const __uint128_t p3 = (__uint128_t)a[3] * b;

        const uint64_t lo0 = (uint64_t)p0;
        const uint64_t lo1 = (uint64_t)p1;
        const uint64_t lo2 = (uint64_t)p2;
        const uint64_t lo3 = (uint64_t)p3;

        uint64_t x0;
        uint64_t x1;
        uint64_t x2;
        uint64_t x3;

        uint64_t c;

        /*
         * First carry-chain:
         *
         * r0 + lo0
         * r1 + hi0
         * r2 + hi1
         * r3 + hi2
         *      hi3
         */
        c = add_carry(
            &x0,
            r[0],
            lo0,
            0
        );

        c = add_carry(
            &x1,
            r[1],
            (uint64_t)(p0 >> 64),
            c
        );

        c = add_carry(
            &x2,
            r[2],
            (uint64_t)(p1 >> 64),
            c
        );

        c = add_carry(
            &x3,
            r[3],
            (uint64_t)(p2 >> 64),
            c
        );

        uint64_t top = (uint64_t)(p3 >> 64) + c;

         /*
          * Second carry-chain:
          *
          * x1 + lo1
          * x2 + lo2
          * x3 + lo3
          */
        c = add_carry(&x1, x1, lo1, 0);
        c = add_carry(&x2, x2, lo2, c);
        c = add_carry(&x3, x3, lo3, c);

        top += c;

        r[0] = x0;
        r[1] = x1;
        r[2] = x2;
        r[3] = x3;

        return top;
    }

    if (n == 5 && a[4] == 0) {
        __uint128_t t;

        t = (__uint128_t)r[0] + (__uint128_t)a[0] * (__uint128_t)b;
        r[0] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[1] + (__uint128_t)a[1] * (__uint128_t)b;
        r[1] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[2] + (__uint128_t)a[2] * (__uint128_t)b;
        r[2] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[3] + (__uint128_t)a[3] * (__uint128_t)b;
        r[3] = (uint64_t)t;
        t >>= 2*MP_N;

        // a[4] == 0, so only propagate carry into r[4].
        t += (__uint128_t)r[4];
        r[4] = (uint64_t)t;

        return (uint64_t)(t >> 2*MP_N);
    }

    // mp_addmul(productX, mq, N=5, np0)
    if (n == 5) {
        __uint128_t t;

        t = (__uint128_t)r[0] + (__uint128_t)a[0] * (__uint128_t)b;
        r[0] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[1] + (__uint128_t)a[1] * (__uint128_t)b;
        r[1] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[2] + (__uint128_t)a[2] * (__uint128_t)b;
        r[2] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[3] + (__uint128_t)a[3] * (__uint128_t)b;
        r[3] = (uint64_t)t;
        t >>= 2*MP_N;

        t += (__uint128_t)r[4] + (__uint128_t)a[4] * (__uint128_t)b;
        r[4] = (uint64_t)t;

        return (uint64_t)(t >> 2*MP_N);
    }

#endif

    __uint128_t carry = 0;

    for (size_t i = 0; i < n; i++) {
        __uint128_t t = (__uint128_t)r[i]
                      + (__uint128_t)a[i] * (__uint128_t)b
                      + carry;
        r[i] = (uint64_t)t;
        carry = t >> 2*MP_N;
    }

    return (uint64_t)carry;

#else
    uint64_t carry = 0;

    for (size_t i = 0; i < n; i++) {
        uint64_t lo, hi;
        mp_mul64(a[i], b, lo, hi);

        uint64_t t = r[i] + lo;
        uint64_t c1 = (t < r[i]);

        uint64_t out = t + carry;
        uint64_t c2 = (out < t);

        r[i] = out;
        carry = hi + c1 + c2;
    }

    return carry;
#endif
}

uint64_t mp_add(uint64_t *r, const uint64_t *a, size_t an, const uint64_t *b, size_t bn) {
#if defined(__SIZEOF_INT128__)
    if (an == 5 && bn == 4) {
        __uint128_t t;

        t = (__uint128_t)a[0] + b[0];
        r[0] = (uint64_t)t;
        t >>= 64;

        t += (__uint128_t)a[1] + b[1];
        r[1] = (uint64_t)t;
        t >>= 64;

        t += (__uint128_t)a[2] + b[2];
        r[2] = (uint64_t)t;
        t >>= 64;

        t += (__uint128_t)a[3] + b[3];
        r[3] = (uint64_t)t;
        t >>= 64;

        t += (__uint128_t)a[4];
        r[4] = (uint64_t)t;

        return (uint64_t)(t >> 64);
    }
#endif

    size_t n = (an > bn) ? an : bn;
    uint64_t carry = 0;

    for (size_t i = 0; i < n; i++) {
        uint64_t ai = (i < an) ? a[i] : 0;
        uint64_t bi = (i < bn) ? b[i] : 0;
        carry = add_carry(&r[i], ai, bi, carry);
    }

    return carry;
}

static inline int mp_is_even(const uint64_t *a) {
    return ((a[0] & 1ULL) == 0ULL);
}

static inline bool mp_is_one(const uint64_t *a) {
    if (a[0] != 1ULL) return false;
    for (int i = 1; i < MP_N64; i++) {
        if (a[i] != 0ULL) return false;
    }
    return true;
}

static inline void mp_sub_mod(uint64_t *x, const uint64_t *y, const uint64_t *mod) {
#if MP_N64 == 4                      //fast path
    uint64_t t0, t1, t2, t3;
    uint64_t br = 0;

    br = sub_borrow(&t0, x[0], y[0], br);
    br = sub_borrow(&t1, x[1], y[1], br);
    br = sub_borrow(&t2, x[2], y[2], br);
    br = sub_borrow(&t3, x[3], y[3], br);

    if (br) {
        uint64_t c = 0;
        c = add_carry(&t0, t0, mod[0], c);
        c = add_carry(&t1, t1, mod[1], c);
        c = add_carry(&t2, t2, mod[2], c);
        c = add_carry(&t3, t3, mod[3], c);
    }

    x[0] = t0; x[1] = t1; x[2] = t2; x[3] = t3;
#else
    uint64_t t[MP_N64];
    uint64_t br = 0;

    for (int i = 0; i < MP_N64; i++) {
        br = sub_borrow(&t[i], x[i], y[i], br);
    }

    if (br) {
        uint64_t c = 0;
        for (int i = 0; i < MP_N64; i++) {
            c = add_carry(&t[i], t[i], mod[i], c);
        }
    }

    mp_copy(x, t);
#endif
}

static inline void mp_shr1(uint64_t *x) {
#if MP_N64 == 4
    const uint64_t x0 = x[0];
    const uint64_t x1 = x[1];
    const uint64_t x2 = x[2];
    const uint64_t x3 = x[3];

    x[0] = (x0 >> 1) | (x1 << 63);
    x[1] = (x1 >> 1) | (x2 << 63);
    x[2] = (x2 >> 1) | (x3 << 63);
    x[3] = (x3 >> 1);
#else
    uint64_t hi = 0;

    for (int i = MP_N64 - 1; i >= 0; --i) {
        uint64_t new_hi = x[i] & 1ULL;
        x[i] = (x[i] >> 1) | (hi << 63);
        hi = new_hi;
    }
#endif
}

static inline void mp_div2_mod(uint64_t *x, const uint64_t *mod) {
#if MP_N64 == 4
    uint64_t hi = 0;

    if (x[0] & 1ULL) {
        uint64_t c = 0;

        c = add_carry(&x[0], x[0], mod[0], c);
        c = add_carry(&x[1], x[1], mod[1], c);
        c = add_carry(&x[2], x[2], mod[2], c);
        c = add_carry(&x[3], x[3], mod[3], c);

        hi = c;
    }

    const uint64_t x0 = x[0];
    const uint64_t x1 = x[1];
    const uint64_t x2 = x[2];
    const uint64_t x3 = x[3];

    x[0] = (x0 >> 1) | (x1 << 63);
    x[1] = (x1 >> 1) | (x2 << 63);
    x[2] = (x2 >> 1) | (x3 << 63);
    x[3] = (x3 >> 1) | (hi << 63);
#else
    uint64_t hi = 0;

    if (x[0] & 1ULL) {
        uint64_t c = 0;
        for (int i = 0; i < MP_N64; ++i) {
            c = add_carry(&x[i], x[i], mod[i], c);
        }
        hi = c;
    }

    for (int i = MP_N64 - 1; i >= 0; --i) {
        uint64_t new_hi = x[i] & 1ULL;
        x[i] = (x[i] >> 1) | (hi << 63);
        hi = new_hi;
    }
#endif
}

static inline bool mp_inv_mod_bin(uint64_t *r, const uint64_t *a, const uint64_t *mod) {
    uint64_t u[MP_N64], v[MP_N64];
    mp_copy(u, a);
    mp_copy(v, mod);

    if (mp_is_zero(u)) { mp_zero(r); return false; }

    uint64_t x1[MP_N64], x2[MP_N64];
    mp_set(x1, 1u);
    mp_zero(x2);

    while (!mp_is_one(u) && !mp_is_one(v)) {

        while (mp_is_even(u)) {
            mp_shr1(u);
            mp_div2_mod(x1, mod);
        }

        while (mp_is_even(v)) {
            mp_shr1(v);
            mp_div2_mod(x2, mod);
        }

        if (mp_cmp(u, v) >= 0) {
            mp_sub(u, u, v);
            mp_sub_mod(x1, x2, mod);   // x1 = x1 - x2 (mod)
        } else {
            mp_sub(v, v, u);
            mp_sub_mod(x2, x1, mod);   // x2 = x2 - x1 (mod)
        }
    }

    if (mp_is_one(u)) mp_copy(r, x1);
    else              mp_copy(r, x2);
    return true;
}

#if MP_N64 == 4 && defined(__SIZEOF_INT128__)

struct MpInvSigned62 {
    int64_t v[5];
};

struct MpInvTrans2x2 {
    int64_t u;
    int64_t v;
    int64_t q;
    int64_t r;
};

static constexpr uint64_t MP_INV_M62_U = UINT64_MAX >> 2;
static constexpr int64_t  MP_INV_M62_S = (int64_t)(UINT64_MAX >> 2);

static inline bool mp_inv_is_zero4(const uint64_t *x) {
    return (x[0] | x[1] | x[2] | x[3]) == 0;
}

static inline uint64_t mp_inv_mod2_62(uint64_t a) {
    uint64_t x = 1;

    x *= 2ULL - a * x;
    x *= 2ULL - a * x;
    x *= 2ULL - a * x;
    x *= 2ULL - a * x;
    x *= 2ULL - a * x;
    x *= 2ULL - a * x;

    return x & MP_INV_M62_U;
}

static inline void mp_inv_to_signed62(MpInvSigned62 *r, const uint64_t *x) {
    r->v[0] = (int64_t)(x[0] & MP_INV_M62_U);
    r->v[1] = (int64_t)((x[0] >> 62) | ((x[1] & ((1ULL << 60) - 1ULL)) << 2));
    r->v[2] = (int64_t)((x[1] >> 60) | ((x[2] & ((1ULL << 58) - 1ULL)) << 4));
    r->v[3] = (int64_t)((x[2] >> 58) | ((x[3] & ((1ULL << 56) - 1ULL)) << 6));
    r->v[4] = (int64_t)(x[3] >> 56);
}

static inline void mp_inv_from_signed62(uint64_t *r, const MpInvSigned62 *x) {
    const uint64_t x0 = (uint64_t)x->v[0];
    const uint64_t x1 = (uint64_t)x->v[1];
    const uint64_t x2 = (uint64_t)x->v[2];
    const uint64_t x3 = (uint64_t)x->v[3];
    const uint64_t x4 = (uint64_t)x->v[4];

    r[0] = x0 | (x1 << 62);
    r[1] = (x1 >> 2) | (x2 << 60);
    r[2] = (x2 >> 4) | (x3 << 58);
    r[3] = (x3 >> 6) | (x4 << 56);
}

static inline int mp_inv_ctz64_var(uint64_t x) {
    return __builtin_ctzll(x);
}

static inline int64_t mp_inv_divsteps_62_var(
    int64_t eta,
    uint64_t f0,
    uint64_t g0,
    MpInvTrans2x2 *t
) {
    uint64_t u = 1;
    uint64_t v = 0;
    uint64_t q = 0;
    uint64_t r = 1;
    uint64_t f = f0;
    uint64_t g = g0;

    int i = 62;

    for (;;) {
        const int zeros = mp_inv_ctz64_var(g | (UINT64_MAX << i));

        g >>= zeros;
        u <<= zeros;
        v <<= zeros;
        eta -= zeros;
        i -= zeros;

        if (i == 0) {
            break;
        }

        int limit;
        uint64_t mask;
        uint32_t w;

        if (eta < 0) {
            uint64_t tmp;

            eta = -eta;

            tmp = f; f = g; g = -tmp;
            tmp = u; u = q; q = -tmp;
            tmp = v; v = r; r = -tmp;

            limit = ((int)eta + 1 > i) ? i : ((int)eta + 1);
            mask = (UINT64_MAX >> (64 - limit)) & 63U;
            w = (uint32_t)((f * g * (f * f - 2U)) & mask);
        } else {
            limit = ((int)eta + 1 > i) ? i : ((int)eta + 1);
            mask = (UINT64_MAX >> (64 - limit)) & 15U;
            w = (uint32_t)(f + (((f + 1U) & 4U) << 1));
            w = (uint32_t)((-(uint64_t)w * g) & mask);
        }

        g += f * w;
        q += u * w;
        r += v * w;
    }

    t->u = (int64_t)u;
    t->v = (int64_t)v;
    t->q = (int64_t)q;
    t->r = (int64_t)r;

    return eta;
}

static inline void mp_inv_update_fg_62_var(
    int len,
    MpInvSigned62 *f,
    MpInvSigned62 *g,
    const MpInvTrans2x2 *t
) {
    const int64_t u = t->u;
    const int64_t v = t->v;
    const int64_t q = t->q;
    const int64_t r = t->r;

    __int128 cf = (__int128)u * f->v[0] + (__int128)v * g->v[0];
    __int128 cg = (__int128)q * f->v[0] + (__int128)r * g->v[0];

    cf >>= 62;
    cg >>= 62;

    for (int i = 1; i < len; ++i) {
        cf += (__int128)u * f->v[i] + (__int128)v * g->v[i];
        cg += (__int128)q * f->v[i] + (__int128)r * g->v[i];

        f->v[i - 1] = (int64_t)((uint64_t)cf & MP_INV_M62_U);
        cf >>= 62;

        g->v[i - 1] = (int64_t)((uint64_t)cg & MP_INV_M62_U);
        cg >>= 62;
    }

    f->v[len - 1] = (int64_t)cf;
    g->v[len - 1] = (int64_t)cg;
}

static inline void mp_inv_update_de_62(
    MpInvSigned62 *d,
    MpInvSigned62 *e,
    const MpInvTrans2x2 *t,
    const MpInvSigned62 *mod,
    uint64_t mod_inv62
) {
    const int64_t d0 = d->v[0];
    const int64_t d1 = d->v[1];
    const int64_t d2 = d->v[2];
    const int64_t d3 = d->v[3];
    const int64_t d4 = d->v[4];

    const int64_t e0 = e->v[0];
    const int64_t e1 = e->v[1];
    const int64_t e2 = e->v[2];
    const int64_t e3 = e->v[3];
    const int64_t e4 = e->v[4];

    const int64_t u = t->u;
    const int64_t v = t->v;
    const int64_t q = t->q;
    const int64_t r = t->r;

    const int64_t sd = d4 >> 63;
    const int64_t se = e4 >> 63;

    int64_t md = (u & sd) + (v & se);
    int64_t me = (q & sd) + (r & se);

    __int128 cd = (__int128)u * d0 + (__int128)v * e0;
    __int128 ce = (__int128)q * d0 + (__int128)r * e0;

    md -= (int64_t)((mod_inv62 * (uint64_t)cd + (uint64_t)md) & MP_INV_M62_U);
    me -= (int64_t)((mod_inv62 * (uint64_t)ce + (uint64_t)me) & MP_INV_M62_U);

    cd += (__int128)mod->v[0] * md;
    ce += (__int128)mod->v[0] * me;
    cd >>= 62;
    ce >>= 62;

    cd += (__int128)u * d1 + (__int128)v * e1 + (__int128)mod->v[1] * md;
    ce += (__int128)q * d1 + (__int128)r * e1 + (__int128)mod->v[1] * me;
    d->v[0] = (int64_t)((uint64_t)cd & MP_INV_M62_U);
    cd >>= 62;
    e->v[0] = (int64_t)((uint64_t)ce & MP_INV_M62_U);
    ce >>= 62;

    cd += (__int128)u * d2 + (__int128)v * e2 + (__int128)mod->v[2] * md;
    ce += (__int128)q * d2 + (__int128)r * e2 + (__int128)mod->v[2] * me;
    d->v[1] = (int64_t)((uint64_t)cd & MP_INV_M62_U);
    cd >>= 62;
    e->v[1] = (int64_t)((uint64_t)ce & MP_INV_M62_U);
    ce >>= 62;

    cd += (__int128)u * d3 + (__int128)v * e3 + (__int128)mod->v[3] * md;
    ce += (__int128)q * d3 + (__int128)r * e3 + (__int128)mod->v[3] * me;
    d->v[2] = (int64_t)((uint64_t)cd & MP_INV_M62_U);
    cd >>= 62;
    e->v[2] = (int64_t)((uint64_t)ce & MP_INV_M62_U);
    ce >>= 62;

    cd += (__int128)u * d4 + (__int128)v * e4 + (__int128)mod->v[4] * md;
    ce += (__int128)q * d4 + (__int128)r * e4 + (__int128)mod->v[4] * me;
    d->v[3] = (int64_t)((uint64_t)cd & MP_INV_M62_U);
    cd >>= 62;
    e->v[3] = (int64_t)((uint64_t)ce & MP_INV_M62_U);
    ce >>= 62;

    d->v[4] = (int64_t)cd;
    e->v[4] = (int64_t)ce;
}

static inline void mp_inv_normalize_62(
    MpInvSigned62 *r,
    int64_t sign,
    const MpInvSigned62 *mod
) {
    int64_t r0 = r->v[0];
    int64_t r1 = r->v[1];
    int64_t r2 = r->v[2];
    int64_t r3 = r->v[3];
    int64_t r4 = r->v[4];

    int64_t cond_add = r4 >> 63;

    r0 += mod->v[0] & cond_add;
    r1 += mod->v[1] & cond_add;
    r2 += mod->v[2] & cond_add;
    r3 += mod->v[3] & cond_add;
    r4 += mod->v[4] & cond_add;

    const int64_t cond_negate = sign >> 63;

    r0 = (r0 ^ cond_negate) - cond_negate;
    r1 = (r1 ^ cond_negate) - cond_negate;
    r2 = (r2 ^ cond_negate) - cond_negate;
    r3 = (r3 ^ cond_negate) - cond_negate;
    r4 = (r4 ^ cond_negate) - cond_negate;

    r1 += r0 >> 62; r0 &= MP_INV_M62_S;
    r2 += r1 >> 62; r1 &= MP_INV_M62_S;
    r3 += r2 >> 62; r2 &= MP_INV_M62_S;
    r4 += r3 >> 62; r3 &= MP_INV_M62_S;

    cond_add = r4 >> 63;

    r0 += mod->v[0] & cond_add;
    r1 += mod->v[1] & cond_add;
    r2 += mod->v[2] & cond_add;
    r3 += mod->v[3] & cond_add;
    r4 += mod->v[4] & cond_add;

    r1 += r0 >> 62; r0 &= MP_INV_M62_S;
    r2 += r1 >> 62; r1 &= MP_INV_M62_S;
    r3 += r2 >> 62; r2 &= MP_INV_M62_S;
    r4 += r3 >> 62; r3 &= MP_INV_M62_S;

    r->v[0] = r0;
    r->v[1] = r1;
    r->v[2] = r2;
    r->v[3] = r3;
    r->v[4] = r4;
}

static bool mp_inv_mod_safegcd_4(uint64_t *r, const uint64_t *a, const uint64_t *mod) {
    if (mp_inv_is_zero4(a)) {
        mp_zero(r);
        return false;
    }

    MpInvSigned62 mod62;
    MpInvSigned62 x;

    mp_inv_to_signed62(&mod62, mod);
    mp_inv_to_signed62(&x, a);

    const uint64_t mod_inv62 = mp_inv_mod2_62((uint64_t)mod62.v[0]);

    MpInvSigned62 d = {{0, 0, 0, 0, 0}};
    MpInvSigned62 e = {{1, 0, 0, 0, 0}};
    MpInvSigned62 f = mod62;
    MpInvSigned62 g = x;

    int len = 5;
    int64_t eta = -1;

    for (int guard = 0; guard < 20; ++guard) {
        MpInvTrans2x2 t;

        eta = mp_inv_divsteps_62_var(eta, (uint64_t)f.v[0], (uint64_t)g.v[0], &t);
        mp_inv_update_de_62(&d, &e, &t, &mod62, mod_inv62);
        mp_inv_update_fg_62_var(len, &f, &g, &t);

        if (g.v[0] == 0) {
            int64_t cond = 0;
            for (int j = 1; j < len; ++j) {
                cond |= g.v[j];
            }
            if (cond == 0) {
                mp_inv_normalize_62(&d, f.v[len - 1], &mod62);
                mp_inv_from_signed62(r, &d);
                return true;
            }
        }

        const int64_t fn = f.v[len - 1];
        const int64_t gn = g.v[len - 1];
        int64_t cond = ((int64_t)len - 2) >> 63;

        cond |= fn ^ (fn >> 63);
        cond |= gn ^ (gn >> 63);

        if (cond == 0) {
            f.v[len - 2] |= (uint64_t)fn << 62;
            g.v[len - 2] |= (uint64_t)gn << 62;
            --len;
        }
    }

    mp_zero(r);
    return false;
}

#endif

bool mp_inv_mod(uint64_t *r, const uint64_t *a, const uint64_t *mod) {
#if MP_N64 == 4 && defined(__SIZEOF_INT128__)
    if ((mod[0] & 1ULL) != 0) {
        return mp_inv_mod_safegcd_4(r, a, mod);
    }
#endif

    return mp_inv_mod_bin(r, a, mod);
}