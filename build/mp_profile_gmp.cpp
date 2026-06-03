#include "gtest/gtest.h"

#include <gmp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

static constexpr size_t MP_N64 = 4;
using Limb = mp_limb_t;
static_assert(sizeof(Limb) == 8, "This benchmark assumes 64-bit GMP limbs");

struct U256 {
    Limb v[MP_N64];
};

struct Bytes32 {
    uint8_t v[32];
};

static constexpr Limb FQ_Q[MP_N64] = {
    static_cast<Limb>(0x3c208c16d87cfd47ULL),
    static_cast<Limb>(0x97816a916871ca8dULL),
    static_cast<Limb>(0xb85045b68181585dULL),
    static_cast<Limb>(0x30644e72e131a029ULL)
};

static volatile uint64_t g_sink = 0;

static inline uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

static inline uint64_t splitmix64_next(uint64_t &state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline void zero_limbs(Limb *r, size_t n = MP_N64) {
    std::memset(r, 0, n * sizeof(Limb));
}

static inline bool is_zero_u256(const Limb *x) {
    return (x[0] | x[1] | x[2] | x[3]) == 0;
}

static inline void make_nonzero(Limb *x) {
    if (is_zero_u256(x)) x[0] = 1;
}

static inline void make_lt_fq(Limb *x) {
    x[3] %= FQ_Q[3];
    make_nonzero(x);
}

static inline size_t num_limbs(const Limb *x) {
    for (size_t i = MP_N64; i > 0; --i) {
        if (x[i - 1] != 0) return i;
    }
    return 0;
}

static inline uint64_t digest_limbs(const Limb *x) {
    uint64_t d = 0xD6E8FEB86659FD93ULL;
    for (size_t i = 0; i < MP_N64; ++i) {
        d ^= static_cast<uint64_t>(x[i]) + 0x9E3779B97F4A7C15ULL + (d << 6) + (d >> 2);
    }
    return d;
}

static inline uint64_t digest_limbs5(const Limb *x) {
    uint64_t d = 0x6A09E667F3BCC909ULL;
    for (size_t i = 0; i < 5; ++i) {
        d ^= static_cast<uint64_t>(x[i]) + 0x9E3779B97F4A7C15ULL + (d << 6) + (d >> 2);
    }
    return d;
}

static inline uint64_t digest_bytes32(const uint8_t *x) {
    uint64_t d = 0xA0761D6478BD642FULL;
    for (size_t i = 0; i < 32; ++i) {
        d ^= static_cast<uint64_t>(x[i]) + 0x9E3779B97F4A7C15ULL + (d << 6) + (d >> 2);
    }
    return d;
}

static inline std::string hex_from_limbs(const Limb *x) {
    char buf[65];
    std::snprintf(buf, sizeof(buf),
                  "%016llx%016llx%016llx%016llx",
                  static_cast<unsigned long long>(x[3]),
                  static_cast<unsigned long long>(x[2]),
                  static_cast<unsigned long long>(x[1]),
                  static_cast<unsigned long long>(x[0]));
    return std::string(buf);
}

static inline void export_be_raw(uint8_t *out, const Limb *x) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(x);
    for (size_t i = 0; i < 32; ++i) out[i] = p[31 - i];
}

static inline void import_be_raw(Limb *out, const uint8_t *in) {
    zero_limbs(out);
    uint8_t *p = reinterpret_cast<uint8_t *>(out);
    for (size_t i = 0; i < 32; ++i) p[i] = in[31 - i];
}

static inline void import_to_mpz(mpz_t z, const Limb *x) {
    mpz_import(z, MP_N64, -1, sizeof(Limb), 0, 0, x);
}

static inline void export_from_mpz(Limb *out, const mpz_t z) {
    zero_limbs(out);
    size_t count = 0;
    mpz_export(out, &count, -1, sizeof(Limb), 0, 0, z);
}

static inline void free_gmp_string(char *s) {
    if (!s) return;
    void (*freefunc)(void *, size_t) = nullptr;
    mp_get_memory_functions(nullptr, nullptr, &freefunc);
    freefunc(s, std::strlen(s) + 1);
}

static uint64_t gmp_add_varlimbs(Limb *r, const Limb *a, size_t an, const Limb *b, size_t bn) {
    zero_limbs(r);
    if (an == 0 && bn == 0) return 0;

    const Limb *longer = a;
    const Limb *shorter = b;
    size_t ln = an;
    size_t sn = bn;
    if (bn > an) {
        longer = b;
        shorter = a;
        ln = bn;
        sn = an;
    }

    Limb carry = 0;
    if (sn > 0) carry = mpn_add_n(r, longer, shorter, sn);
    for (size_t i = sn; i < ln; ++i) {
        Limb old = longer[i];
        r[i] = old + carry;
        carry = (carry && r[i] < old) ? 1 : 0;
    }
    return static_cast<uint64_t>(carry);
}

static void gmp_shl(Limb *r, const Limb *a, uint64_t k) {
    zero_limbs(r);
    if (k >= MP_N64 * 64u) return;
    if (k == 0) {
        std::memmove(r, a, MP_N64 * sizeof(Limb));
        return;
    }
    const size_t word_shift = static_cast<size_t>(k >> 6);
    const unsigned bit_shift = static_cast<unsigned>(k & 63u);
    const size_t n = MP_N64 - word_shift;
    if (bit_shift == 0) {
        std::memmove(r + word_shift, a, n * sizeof(Limb));
    } else {
        mpn_lshift(r + word_shift, a, n, bit_shift);
    }
}

static void gmp_shr(Limb *r, const Limb *a, uint64_t k) {
    zero_limbs(r);
    if (k >= MP_N64 * 64u) return;
    if (k == 0) {
        std::memmove(r, a, MP_N64 * sizeof(Limb));
        return;
    }
    const size_t word_shift = static_cast<size_t>(k >> 6);
    const unsigned bit_shift = static_cast<unsigned>(k & 63u);
    const size_t n = MP_N64 - word_shift;
    if (bit_shift == 0) {
        std::memmove(r, a + word_shift, n * sizeof(Limb));
    } else {
        mpn_rshift(r, a + word_shift, n, bit_shift);
    }
}

static void gmp_div_256(Limb *q, Limb *r, const Limb *num, const Limb *den) {
    zero_limbs(q);
    zero_limbs(r);

    const size_t nn = num_limbs(num);
    const size_t dn = num_limbs(den);
    if (dn == 0) return;
    if (nn < dn) {
        std::memmove(r, num, MP_N64 * sizeof(Limb));
        return;
    }

    Limb qtmp[MP_N64] = {0, 0, 0, 0};
    Limb rtmp[MP_N64] = {0, 0, 0, 0};
    mpn_tdiv_qr(qtmp, rtmp, 0, num, nn, den, dn);
    std::memmove(q, qtmp, MP_N64 * sizeof(Limb));
    std::memmove(r, rtmp, dn * sizeof(Limb));
}

static void gmp_pow_mod_u256(Limb *r, const Limb *base, const Limb *exp, const Limb *mod) {
    mpz_t bz, ez, mz, rz;
    mpz_inits(bz, ez, mz, rz, nullptr);
    import_to_mpz(bz, base);
    import_to_mpz(ez, exp);
    import_to_mpz(mz, mod);
    mpz_powm(rz, bz, ez, mz);
    export_from_mpz(r, rz);
    mpz_clears(bz, ez, mz, rz, nullptr);
}

static bool gmp_inv_mod_u256(Limb *r, const Limb *a, const Limb *mod) {
    mpz_t az, mz, rz;
    mpz_inits(az, mz, rz, nullptr);
    import_to_mpz(az, a);
    import_to_mpz(mz, mod);
    const int ok = mpz_invert(rz, az, mz);
    if (ok) export_from_mpz(r, rz);
    else zero_limbs(r);
    mpz_clears(az, mz, rz, nullptr);
    return ok != 0;
}

static size_t env_size(const char *name, size_t fallback) {
    const char *p = std::getenv(name);
    if (!p || !*p) return fallback;
    char *end = nullptr;
    unsigned long long v = std::strtoull(p, &end, 10);
    if (!end || *end != '\0' || v == 0) return fallback;
    return static_cast<size_t>(v);
}

static uint64_t env_u64(const char *name, uint64_t fallback) {
    const char *p = std::getenv(name);
    if (!p || !*p) return fallback;
    char *end = nullptr;
    unsigned long long v = std::strtoull(p, &end, 0);
    if (!end || *end != '\0') return fallback;
    return static_cast<uint64_t>(v);
}

struct Dataset {
    std::vector<U256> a;
    std::vector<U256> b;
    std::vector<U256> a_mod;
    std::vector<Bytes32> be;
    std::vector<uint64_t> word;
    std::vector<uint64_t> shift;
    std::vector<int64_t> signed_word;
    std::vector<uint8_t> len_a;
    std::vector<uint8_t> len_b;
    std::vector<uint8_t> len_n;
    std::vector<std::string> hex;
};

static Dataset make_dataset(size_t n, uint64_t seed) {
    Dataset d;
    d.a.resize(n);
    d.b.resize(n);
    d.a_mod.resize(n);
    d.be.resize(n);
    d.word.resize(n);
    d.shift.resize(n);
    d.signed_word.resize(n);
    d.len_a.resize(n);
    d.len_b.resize(n);
    d.len_n.resize(n);
    d.hex.reserve(n);

    uint64_t st = seed;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < MP_N64; ++j) {
            d.a[i].v[j] = static_cast<Limb>(splitmix64_next(st));
            d.b[i].v[j] = static_cast<Limb>(splitmix64_next(st));
        }
        make_nonzero(d.b[i].v);

        std::memcpy(d.a_mod[i].v, d.a[i].v, sizeof(d.a[i].v));
        make_lt_fq(d.a_mod[i].v);

        d.word[i] = splitmix64_next(st);
        d.shift[i] = splitmix64_next(st) % 320u;
        d.signed_word[i] = static_cast<int64_t>(splitmix64_next(st));
        d.len_a[i] = static_cast<uint8_t>(splitmix64_next(st) % (MP_N64 + 1));
        d.len_b[i] = static_cast<uint8_t>(splitmix64_next(st) % (MP_N64 + 1));
        d.len_n[i] = static_cast<uint8_t>((splitmix64_next(st) % MP_N64) + 1);

        export_be_raw(d.be[i].v, d.a[i].v);
        d.hex.push_back(hex_from_limbs(d.a[i].v));
    }
    return d;
}

struct Stats {
    const char *name;
    size_t n;
    uint64_t total_ns;
    double avg_ns;
    double block_mean_ns;
    double block_stddev_ns;
    double block_cv;
    double block_min_ns;
    double block_p50_ns;
    double block_p95_ns;
    double block_p99_ns;
    double block_max_ns;
    uint64_t digest;
};

static double percentile_sorted(const std::vector<double> &v, double p) {
    if (v.empty()) return 0.0;
    const double idx = p * static_cast<double>(v.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return v[lo];
    const double t = idx - static_cast<double>(lo);
    return v[lo] * (1.0 - t) + v[hi] * t;
}

template <class Fn>
static Stats run_bench(const char *name, size_t n, size_t block, Fn &&fn) {
    std::vector<double> samples;
    samples.reserve((n + block - 1) / block);

    uint64_t digest = 0x123456789ABCDEF0ULL;
    const uint64_t total_t0 = now_ns();

    for (size_t base = 0; base < n; base += block) {
        const size_t end = std::min(n, base + block);
        const uint64_t t0 = now_ns();
        for (size_t i = base; i < end; ++i) {
            const uint64_t x = fn(i);
            digest ^= x + 0x9E3779B97F4A7C15ULL + (digest << 6) + (digest >> 2);
        }
        const uint64_t t1 = now_ns();
        samples.push_back(static_cast<double>(t1 - t0) / static_cast<double>(end - base));
    }

    const uint64_t total_t1 = now_ns();
    g_sink ^= digest;

    std::sort(samples.begin(), samples.end());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
    double var = 0.0;
    for (double x : samples) var += (x - mean) * (x - mean);
    var = samples.empty() ? 0.0 : var / static_cast<double>(samples.size());
    const double sd = std::sqrt(var);

    Stats s{};
    s.name = name;
    s.n = n;
    s.total_ns = total_t1 - total_t0;
    s.avg_ns = static_cast<double>(s.total_ns) / static_cast<double>(n);
    s.block_mean_ns = mean;
    s.block_stddev_ns = sd;
    s.block_cv = mean == 0.0 ? 0.0 : sd / mean;
    s.block_min_ns = samples.empty() ? 0.0 : samples.front();
    s.block_p50_ns = percentile_sorted(samples, 0.50);
    s.block_p95_ns = percentile_sorted(samples, 0.95);
    s.block_p99_ns = percentile_sorted(samples, 0.99);
    s.block_max_ns = samples.empty() ? 0.0 : samples.back();
    s.digest = digest;
    return s;
}

static void print_header() {
    std::cout
        << "impl,function,n,total_ns,avg_ns_per_call,block_mean_ns_per_call,"
        << "block_stddev_ns_per_call,block_cv,block_min_ns_per_call,"
        << "block_p50_ns_per_call,block_p95_ns_per_call,block_p99_ns_per_call,"
        << "block_max_ns_per_call,digest\n";
}

static void print_stats(const char *impl, const Stats &s) {
    std::cout << impl << ',' << s.name << ',' << s.n << ',' << s.total_ns << ','
              << std::fixed << std::setprecision(3)
              << s.avg_ns << ',' << s.block_mean_ns << ',' << s.block_stddev_ns << ','
              << s.block_cv << ',' << s.block_min_ns << ',' << s.block_p50_ns << ','
              << s.block_p95_ns << ',' << s.block_p99_ns << ',' << s.block_max_ns << ','
              << std::hex << s.digest << std::dec << '\n';
}

} // namespace

TEST(MPProfileGMP, MillionPointTiming) {
    const size_t n = env_size("MP_BENCH_N", 1000000);
    const size_t block = env_size("MP_BENCH_BLOCK", 1024);
    const uint64_t seed = env_u64("MP_BENCH_SEED", 0xBADC0FFEE0DDF00DULL);
    const Dataset d = make_dataset(n, seed);

    print_header();

    auto emit = [&](const char *name, auto &&fn) {
        print_stats("gmp", run_bench(name, n, block, std::forward<decltype(fn)>(fn)));
    };

    emit("mp_zero", [&](size_t) -> uint64_t {
        Limb r[MP_N64];
        zero_limbs(r);
        return digest_limbs(r);
    });

    emit("mp_set_u64", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        zero_limbs(r);
        r[0] = static_cast<Limb>(d.word[i]);
        return digest_limbs(r);
    });

    emit("mp_copy", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        std::memmove(r, d.a[i].v, MP_N64 * sizeof(Limb));
        return digest_limbs(r);
    });

    emit("mp_cmp", [&](size_t i) -> uint64_t {
        return static_cast<uint64_t>(static_cast<int64_t>(mpn_cmp(d.a[i].v, d.b[i].v, MP_N64)) + 2);
    });

    emit("mp_is_zero", [&](size_t i) -> uint64_t {
        return is_zero_u256(d.a[i].v) ? 1u : 0u;
    });

    emit("mp_add_256", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb c = mpn_add_n(r, d.a[i].v, d.b[i].v, MP_N64);
        return digest_limbs(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_add_varlimbs", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        uint64_t c = gmp_add_varlimbs(r, d.a[i].v, d.len_a[i], d.b[i].v, d.len_b[i]);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_add_an5_bn4", [&](size_t i) -> uint64_t {
        Limb a5[5] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3],
            static_cast<Limb>(d.word[i] ^ 0xA0761D6478BD642FULL)
        };

        Limb b4[4] = {
            d.b[i].v[0],
            d.b[i].v[1],
            d.b[i].v[2],
            d.b[i].v[3]
        };

        Limb r5[5] = {0, 0, 0, 0, 0};

        Limb c = mpn_add_n(r5, a5, b4, 4);

        uint64_t top_before = static_cast<uint64_t>(a5[4]);
        r5[4] = a5[4] + c;
        c = (c && static_cast<uint64_t>(r5[4]) < top_before) ? 1 : 0;

        return digest_limbs5(r5) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_sub_256", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb b = mpn_sub_n(r, d.a[i].v, d.b[i].v, MP_N64);
        return digest_limbs(r) ^ (static_cast<uint64_t>(b) << 1);
    });

    emit("mp_add_u64", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb c = mpn_add_1(r, d.a[i].v, MP_N64, static_cast<Limb>(d.word[i]));
        return digest_limbs(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_sub_u64", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb b = mpn_sub_1(r, d.a[i].v, MP_N64, static_cast<Limb>(d.word[i]));
        return digest_limbs(r) ^ (static_cast<uint64_t>(b) << 1);
    });

    emit("mp_mul_u64", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb c = mpn_mul_1(r, d.a[i].v, MP_N64, static_cast<Limb>(d.word[i]));
        return digest_limbs(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_addmul_u64", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        std::memmove(r, d.a[i].v, MP_N64 * sizeof(Limb));
        Limb c = mpn_addmul_1(r, d.b[i].v, d.len_n[i], static_cast<Limb>(d.word[i]));
        return digest_limbs(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_addmul_n4", [&](size_t i) -> uint64_t {
        Limb r[4] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3]
        };

        const Limb c = mpn_addmul_1(
            r,
            d.b[i].v,
            4,
            static_cast<Limb>(d.word[i])
        );

        return digest_limbs(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_addmul_n5_qzero", [&](size_t i) -> uint64_t {
        Limb r[5] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3],
            static_cast<Limb>(d.word[i] ^ 0xD1B54A32D192ED03ULL)
        };

        Limb a5[5] = {
            FQ_Q[0],
            FQ_Q[1],
            FQ_Q[2],
            FQ_Q[3],
            0
        };

        const Limb c = mpn_addmul_1(
            r,
            a5,
            5,
            static_cast<Limb>(d.word[i])
        );

        return digest_limbs5(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_addmul_n5_full", [&](size_t i) -> uint64_t {
        Limb r[5] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3],
            static_cast<Limb>(d.word[i] ^ 0x94D049BB133111EBULL)
        };

        Limb a5[5] = {
            d.b[i].v[0],
            d.b[i].v[1],
            d.b[i].v[2],
            d.b[i].v[3],
            static_cast<Limb>(d.word[i] ^ 0xBF58476D1CE4E5B9ULL)
        };

        const Limb c = mpn_addmul_1(
            r,
            a5,
            5,
            static_cast<Limb>(d.word[i])
        );

        return digest_limbs5(r) ^ (static_cast<uint64_t>(c) << 1);
    });

    emit("mp_and", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        for (size_t j = 0; j < MP_N64; ++j) r[j] = d.a[i].v[j] & d.b[i].v[j];
        return digest_limbs(r);
    });

    emit("mp_or", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        for (size_t j = 0; j < MP_N64; ++j) r[j] = d.a[i].v[j] | d.b[i].v[j];
        return digest_limbs(r);
    });

    emit("mp_xor", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        for (size_t j = 0; j < MP_N64; ++j) r[j] = d.a[i].v[j] ^ d.b[i].v[j];
        return digest_limbs(r);
    });

    emit("mp_not", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        for (size_t j = 0; j < MP_N64; ++j) r[j] = ~d.a[i].v[j];
        return digest_limbs(r);
    });

    emit("mp_tstbit", [&](size_t i) -> uint64_t {
        const uint64_t bit = d.shift[i];
        if (bit >= MP_N64 * 64u) return 0u;
        return static_cast<uint64_t>((d.a[i].v[bit >> 6] >> (bit & 63u)) & 1u);
    });

    emit("mp_shl", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        gmp_shl(r, d.a[i].v, d.shift[i]);
        return digest_limbs(r);
    });

    emit("mp_shr", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        gmp_shr(r, d.a[i].v, d.shift[i]);
        return digest_limbs(r);
    });

    emit("mp_get_int32", [&](size_t i) -> uint64_t {
        return static_cast<uint32_t>(static_cast<int32_t>(d.a[i].v[0]));
    });

    emit("mp_fits_int32", [&](size_t i) -> uint64_t {
        bool ok = d.a[i].v[0] <= static_cast<Limb>(INT32_MAX);
        for (size_t j = 1; j < MP_N64; ++j) ok = ok && d.a[i].v[j] == 0;
        return ok ? 1u : 0u;
    });

    emit("mp_set_str_hex", [&](size_t i) -> uint64_t {
        mpz_t z;
        mpz_init(z);
        const int ok = mpz_set_str(z, d.hex[i].c_str(), 16);
        Limb r[MP_N64];
        export_from_mpz(r, z);
        mpz_clear(z);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok == 0);
    });

    emit("mp_get_str_hex", [&](size_t i) -> uint64_t {
        mpz_t z;
        mpz_init(z);
        import_to_mpz(z, d.a[i].v);
        char *s = mpz_get_str(nullptr, 16, z);
        uint64_t out = static_cast<uint64_t>(std::strlen(s)) ^ (s[0] ? static_cast<uint8_t>(s[0]) : 0u);
        free_gmp_string(s);
        mpz_clear(z);
        return out;
    });

    emit("mp_get_str_dec", [&](size_t i) -> uint64_t {
        mpz_t z;
        mpz_init(z);
        import_to_mpz(z, d.a[i].v);
        char *s = mpz_get_str(nullptr, 10, z);
        uint64_t out = static_cast<uint64_t>(std::strlen(s)) ^ (s[0] ? static_cast<uint8_t>(s[0]) : 0u);
        free_gmp_string(s);
        mpz_clear(z);
        return out;
    });

    emit("mp_set_mod_i64", [&](size_t i) -> uint64_t {
        mpz_t z, m;
        mpz_inits(z, m, nullptr);
        mpz_set_si(z, d.signed_word[i]);
        import_to_mpz(m, FQ_Q);
        mpz_mod(z, z, m);
        Limb r[MP_N64];
        export_from_mpz(r, z);
        mpz_clears(z, m, nullptr);
        return digest_limbs(r);
    });

    emit("mp_set_mod_str_hex", [&](size_t i) -> uint64_t {
        mpz_t z, m;
        mpz_inits(z, m, nullptr);
        const int ok = mpz_set_str(z, d.hex[i].c_str(), 16);
        import_to_mpz(m, FQ_Q);
        mpz_mod(z, z, m);
        Limb r[MP_N64];
        export_from_mpz(r, z);
        mpz_clears(z, m, nullptr);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok == 0);
    });

    emit("mp_export_be", [&](size_t i) -> uint64_t {
        uint8_t out[32];
        export_be_raw(out, d.a[i].v);
        return digest_bytes32(out);
    });

    emit("mp_import_be", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        import_be_raw(r, d.be[i].v);
        return digest_limbs(r);
    });

    emit("mp_div", [&](size_t i) -> uint64_t {
        Limb q[MP_N64], r[MP_N64];
        gmp_div_256(q, r, d.a[i].v, d.b[i].v);
        return digest_limbs(q) ^ (digest_limbs(r) << 1);
    });

    emit("mp_pow_mod_exp17", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        Limb e[MP_N64] = {static_cast<Limb>(17), 0, 0, 0};
        gmp_pow_mod_u256(r, d.a_mod[i].v, e, FQ_Q);
        return digest_limbs(r);
    });

    emit("mp_inv_mod_fq", [&](size_t i) -> uint64_t {
        Limb r[MP_N64];
        bool ok = gmp_inv_mod_u256(r, d.a_mod[i].v, FQ_Q);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok);
    });

    std::cerr << "g_sink=" << std::hex << g_sink << std::dec << "\n";
    SUCCEED();
}
