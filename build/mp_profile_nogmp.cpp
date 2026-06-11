#include "mp.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

static_assert(MP_N64 == 4, "This benchmark assumes 4x64-bit limbs");

struct U256 {
    uint64_t v[MP_N64];
};

struct Bytes32 {
    uint8_t v[32];
};

static constexpr uint64_t FQ_Q[MP_N64] = {
    0x3c208c16d87cfd47ULL,
    0x97816a916871ca8dULL,
    0xb85045b68181585dULL,
    0x30644e72e131a029ULL
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

static inline bool is_zero_u256(const uint64_t *x) {
    return (x[0] | x[1] | x[2] | x[3]) == 0;
}

static inline void make_nonzero(uint64_t *x) {
    if (is_zero_u256(x)) x[0] = 1;
}

static inline void make_lt_fq(uint64_t *x) {
    // Since x[3] is strictly below FQ_Q[3], the whole 256-bit value is < FQ_Q.
    x[3] %= FQ_Q[3];
    make_nonzero(x);
}

static inline uint64_t digest_limbs(const uint64_t *x) {
    uint64_t d = 0xD6E8FEB86659FD93ULL;
    for (size_t i = 0; i < MP_N64; ++i) {
        d ^= x[i] + 0x9E3779B97F4A7C15ULL + (d << 6) + (d >> 2);
    }
    return d;
}

static inline uint64_t digest_limbs5(const uint64_t *x) {
    uint64_t d = 0x6A09E667F3BCC909ULL;
    for (size_t i = 0; i < 5; ++i) {
        d ^= x[i] + 0x9E3779B97F4A7C15ULL + (d << 6) + (d >> 2);
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

static inline std::string hex_from_limbs(const uint64_t *x) {
    char buf[65];
    std::snprintf(buf, sizeof(buf),
                  "%016llx%016llx%016llx%016llx",
                  static_cast<unsigned long long>(x[3]),
                  static_cast<unsigned long long>(x[2]),
                  static_cast<unsigned long long>(x[1]),
                  static_cast<unsigned long long>(x[0]));
    return std::string(buf);
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
            d.a[i].v[j] = splitmix64_next(st);
            d.b[i].v[j] = splitmix64_next(st);
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

        mp_export_be(d.be[i].v, d.a[i].v);
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

TEST(MPProfileNoGMP, MillionPointTiming) {
    const size_t n = env_size("MP_BENCH_N", 1000000);
    const size_t block = env_size("MP_BENCH_BLOCK", 1024);
    const uint64_t seed = env_u64("MP_BENCH_SEED", 0xBADC0FFEE0DDF00DULL);
    const Dataset d = make_dataset(n, seed);

    print_header();

    auto emit = [&](const char *name, auto &&fn) {
        print_stats("nogmp", run_bench(name, n, block, std::forward<decltype(fn)>(fn)));
    };

    emit("mp_zero", [&](size_t) -> uint64_t {
        mp_uint_t r;
        mp_zero(r);
        return digest_limbs(r);
    });

    emit("mp_set_u64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_set(r, d.word[i]);
        return digest_limbs(r);
    });

    emit("mp_copy", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_copy(r, d.a[i].v);
        return digest_limbs(r);
    });

    emit("mp_cmp", [&](size_t i) -> uint64_t {
        return static_cast<uint64_t>(static_cast<int64_t>(mp_cmp(d.a[i].v, d.b[i].v)) + 2);
    });

    emit("mp_is_zero", [&](size_t i) -> uint64_t {
        return mp_is_zero(d.a[i].v) ? 1u : 0u;
    });

    emit("mp_add_256", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        uint64_t c = mp_add(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_add_varlimbs", [&](size_t i) -> uint64_t {
        uint64_t r[MP_N64] = {0, 0, 0, 0};
        uint64_t c = mp_add(r, d.a[i].v, d.len_a[i], d.b[i].v, d.len_b[i]);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_add_an5_bn4", [&](size_t i) -> uint64_t {
    uint64_t a5[5] = {
        d.a[i].v[0],
        d.a[i].v[1],
        d.a[i].v[2],
        d.a[i].v[3],
        d.word[i] ^ 0xA0761D6478BD642FULL
    };

    uint64_t b4[4] = {
        d.b[i].v[0],
        d.b[i].v[1],
        d.b[i].v[2],
        d.b[i].v[3]
    };

    uint64_t r5[5] = {0, 0, 0, 0, 0};

    const uint64_t c = mp_add(r5, a5, 5, b4, 4);

    return digest_limbs5(r5) ^ (c << 1);
});

    emit("mp_sub_256", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        uint64_t b = mp_sub(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r) ^ (b << 1);
    });

    emit("mp_add_u64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        uint64_t c = mp_add(r, d.a[i].v, d.word[i]);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_sub_u64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        uint64_t b = mp_sub(r, d.a[i].v, d.word[i]);
        return digest_limbs(r) ^ (b << 1);
    });

    emit("mp_mul_u64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        uint64_t c = mp_mul(r, d.a[i].v, d.word[i]);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_addmul_u64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_copy(r, d.a[i].v);
        uint64_t c = mp_addmul(r, d.b[i].v, d.len_n[i], d.word[i]);
        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_addmul_n4", [&](size_t i) -> uint64_t {
        uint64_t r[4] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3]
        };

        const uint64_t c = mp_addmul(r, d.b[i].v, 4, d.word[i]);

        return digest_limbs(r) ^ (c << 1);
    });

    emit("mp_addmul_n5_qzero", [&](size_t i) -> uint64_t {
        uint64_t r[5] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3],
            d.word[i] ^ 0xD1B54A32D192ED03ULL
        };

        // Montgomery-style source: 4 real modulus limbs + one zero top limb.
        // This matches Fq_rawq/Fr_rawq layout: q[4] = 0.
        uint64_t a5[5] = {
            FQ_Q[0],
            FQ_Q[1],
            FQ_Q[2],
            FQ_Q[3],
            0
        };

        const uint64_t c = mp_addmul(r, a5, 5, d.word[i]);

        return digest_limbs5(r) ^ (c << 1);
    });

    emit("mp_addmul_n5_full", [&](size_t i) -> uint64_t {
        uint64_t r[5] = {
            d.a[i].v[0],
            d.a[i].v[1],
            d.a[i].v[2],
            d.a[i].v[3],
            d.word[i] ^ 0x94D049BB133111EBULL
        };

        uint64_t a5[5] = {
            d.b[i].v[0],
            d.b[i].v[1],
            d.b[i].v[2],
            d.b[i].v[3],
            d.word[i] ^ 0xBF58476D1CE4E5B9ULL
        };

        const uint64_t c = mp_addmul(r, a5, 5, d.word[i]);

        return digest_limbs5(r) ^ (c << 1);
    });

    emit("mp_and", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_and(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    });

    emit("mp_or", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_or(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    });

    emit("mp_xor", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_xor(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    });

    emit("mp_not", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_not(r, d.a[i].v);
        return digest_limbs(r);
    });

    emit("mp_tstbit", [&](size_t i) -> uint64_t {
        return mp_tstbit(d.a[i].v, d.shift[i]) ? 1u : 0u;
    });

    emit("mp_shl", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_shl(r, d.a[i].v, d.shift[i]);
        return digest_limbs(r);
    });

    emit("mp_shr", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_shr(r, d.a[i].v, d.shift[i]);
        return digest_limbs(r);
    });

    emit("mp_get_int32", [&](size_t i) -> uint64_t {
        return static_cast<uint32_t>(mp_get_int32(d.a[i].v));
    });

    emit("mp_fits_int32", [&](size_t i) -> uint64_t {
        return mp_fits_int32(d.a[i].v) ? 1u : 0u;
    });

    emit("mp_set_str_hex", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        bool ok = mp_set(r, d.hex[i].c_str(), 16);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok);
    });

    emit("mp_get_str_hex", [&](size_t i) -> uint64_t {
        std::string s = mp_get_str(d.a[i].v, 16);
        return static_cast<uint64_t>(s.size()) ^ (s.empty() ? 0u : static_cast<uint8_t>(s[0]));
    });

    emit("mp_get_str_dec", [&](size_t i) -> uint64_t {
        std::string s = mp_get_str(d.a[i].v, 10);
        return static_cast<uint64_t>(s.size()) ^ (s.empty() ? 0u : static_cast<uint8_t>(s[0]));
    });

    emit("mp_set_mod_i64", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_set_mod(r, d.signed_word[i], FQ_Q);
        return digest_limbs(r);
    });

    emit("mp_set_mod_str_hex", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        bool ok = mp_set_mod(r, d.hex[i].c_str(), 16, FQ_Q);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok);
    });

    emit("mp_export_be", [&](size_t i) -> uint64_t {
        uint8_t out[32];
        mp_export_be(out, d.a[i].v);
        return digest_bytes32(out);
    });

    emit("mp_import_be", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_import_be(r, d.be[i].v);
        return digest_limbs(r);
    });

    emit("mp_div", [&](size_t i) -> uint64_t {
        mp_uint_t q, r;
        mp_div(q, r, d.a[i].v, d.b[i].v);
        return digest_limbs(q) ^ (digest_limbs(r) << 1);
    });

    emit("mp_pow_mod_exp17", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_uint_t e;
        mp_set(e, 17);
        mp_pow_mod(r, d.a_mod[i].v, e, FQ_Q);
        return digest_limbs(r);
    });

    emit("mp_inv_mod_fq", [&](size_t i) -> uint64_t {
        mp_uint_t r;
        bool ok = mp_inv_mod(r, d.a_mod[i].v, FQ_Q);
        return digest_limbs(r) ^ static_cast<uint64_t>(ok);
    });

    std::cerr << "g_sink=" << std::hex << g_sink << std::dec << "\n";
    SUCCEED();
}

TEST(MPProfileNoGMP, PowModExponentTiming) {
    const size_t n = env_size("MP_POW_BENCH_N", 10000);
    const size_t block = env_size("MP_BENCH_BLOCK", 64);
    const uint64_t seed = env_u64("MP_BENCH_SEED", 0xBADC0FFEE0DDF00DULL);
    const Dataset d = make_dataset(n, seed);

    struct ExpCase {
        const char *name;
        uint64_t e[MP_N64];
    };

    const ExpCase cases[] = {
        {"mp_pow_mod_exp17",        {17ULL, 0ULL, 0ULL, 0ULL}},
        {"mp_pow_mod_exp65537",     {65537ULL, 0ULL, 0ULL, 0ULL}},
        {"mp_pow_mod_sparse_256",   {1ULL, 0ULL, 0ULL, 0x8000000000000000ULL}},
        {"mp_pow_mod_dense_128",    {~0ULL, ~0ULL, 0ULL, 0ULL}},
        {"mp_pow_mod_dense_256",    {~0ULL, ~0ULL, ~0ULL, ~0ULL}},
        {"mp_pow_mod_fq_q_minus_2", {
            FQ_Q[0] - 2ULL,
            FQ_Q[1],
            FQ_Q[2],
            FQ_Q[3]
        }},
    };

    print_header();

    for (const auto &tc : cases) {
        print_stats("nogmp", run_bench(tc.name, n, block, [&](size_t i) -> uint64_t {
            mp_uint_t r;
            mp_uint_t e;
            mp_copy(e, tc.e);
            mp_pow_mod(r, d.a_mod[i].v, e, FQ_Q);
            return digest_limbs(r);
        }));
    }

    std::cerr << "g_sink=" << std::hex << g_sink << std::dec << "\n";
    SUCCEED();
}

TEST(MPProfileNoGMP, PowModRandomBigTiming) {
    const size_t n = env_size("MP_RANDOM_POW_BENCH_N", 1000000);
    const size_t block = env_size("MP_BENCH_BLOCK", 64);
    const uint64_t seed = env_u64("MP_BENCH_SEED", 0xBADC0FFEE0DDF00DULL);
    const Dataset d = make_dataset(n, seed);

    print_header();

    print_stats("nogmp", run_bench("mp_pow_mod_random_big_256", n, block, [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_uint_t e;

        mp_copy(e, d.b[i].v);

        // Force a real 256-bit exponent, not accidentally small.
        e[3] |= 0x8000000000000000ULL;

        mp_pow_mod(r, d.a_mod[i].v, e, FQ_Q);

        return digest_limbs(r);
    }));

    std::cerr << "g_sink=" << std::hex << g_sink << std::dec << "\n";
    SUCCEED();
}

static uint64_t mp_tune_digest_string_local(const std::string &s) {
    uint64_t h = 1469598103934665603ULL;

    for (unsigned char c : s) {
        h ^= (uint64_t)c;
        h *= 1099511628211ULL;
    }

    h ^= (uint64_t)s.size();
    h *= 1099511628211ULL;

    return h;
}

TEST(MPProfileNoGMP, StringBitwiseFocusTiming) {
    const size_t n = env_size("MP_TUNE_BENCH_N", 1000000);
    const size_t block = env_size("MP_BENCH_BLOCK", 1024);
    const uint64_t seed = env_u64("MP_BENCH_SEED", 0xBADC0FFEE0DDF00DULL);
    const Dataset d = make_dataset(n, seed);

    print_header();

    print_stats("nogmp", run_bench("mp_get_str_dec_focus", n, block, [&](size_t i) -> uint64_t {
        std::string s = mp_get_str(d.a[i].v, 10);
        return mp_tune_digest_string_local(s);
    }));

    print_stats("nogmp", run_bench("mp_and_focus", n, block, [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_and(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    }));

    print_stats("nogmp", run_bench("mp_or_focus", n, block, [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_or(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    }));

    print_stats("nogmp", run_bench("mp_xor_focus", n, block, [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_xor(r, d.a[i].v, d.b[i].v);
        return digest_limbs(r);
    }));

    print_stats("nogmp", run_bench("mp_not_focus", n, block, [&](size_t i) -> uint64_t {
        mp_uint_t r;
        mp_not(r, d.a[i].v);
        return digest_limbs(r);
    }));

    print_stats("nogmp", run_bench("mp_tstbit_focus", n, block, [&](size_t i) -> uint64_t {
        const size_t bit = (size_t)(d.b[i].v[0] & 255ULL);
        const bool b = mp_tstbit(d.a[i].v, bit);

        return b
            ? 0x9e3779b97f4a7c15ULL ^ (uint64_t)bit
            : 0xd1b54a32d192ed03ULL ^ (uint64_t)bit;
    }));

    std::cerr << "g_sink=" << std::hex << g_sink << std::dec << "\n";
    SUCCEED();
}