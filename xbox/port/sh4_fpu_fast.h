/* the port's own copy of xbox/third_party/sh4_fpu_approx/'s FSRRA (MIT,
 * Stefanos Kornilios Mitsis Poiitidis), bit for bit the same results, for
 * the game's FPSCR alone (rounding toward zero, denormals as zero) and in
 * 32-bit integers wherever the values fit, as the Xbox's Pentium III does
 * 64-bit integer work slowly. the vendored header stays unchanged as the
 * reference: xbox/test/fpu_exact.sh checks this one against it (the
 * multiplier array over random operands, FSRRA over every mantissa of both
 * exponent parities).
 *
 * what changes, and why each gives the same bits:
 * - the multiplier array (sh4_fpu__arr): each of its nine Booth rows is
 *   x * 4^k, x = +-|d| * mul or its one's complement, which fits in 27
 *   bits, so floor(row / 2^16), as sh4_fpu__asr takes it, is x >> (16 - 2k)
 *   in 32 bits (2k <= 16), and its bits 15 and 16 are x's bits 15 - 2k and
 *   16 - 2k. the rows' sum fits in 30 bits. only the exact part, (rec -
 *   slo) * mul, needs 64 bits: one 32 by 32 multiply. where rec's low 18
 *   bits are 0 every digit is 0, and the product is rec * mul alone;
 * - FSRRA: its latches fit in 32 bits, and each array product is shifted
 *   down from 64 bits once */
#ifndef XBOX_SH4_FPU_FAST_H
#define XBOX_SH4_FPU_FAST_H
#include "xbox/third_party/sh4_fpu_approx/sh4_fpu_approx.h"

/* sh4_fpu__arr(rec, mul) for rec within 28 bits and mul within 25, signed,
 * as every call of FSRRA's has them */
static inline int64_t sh4_fast_arr(int32_t rec, int32_t mul)
{
    uint32_t lo = (uint32_t)rec & 0x3ffffu;
    /* digits 0 to 8's value, the low 18 bits signed */
    int32_t slo = (int32_t)(lo << 14) >> 14;
    int64_t exact = (int64_t)(rec - slo) * (int64_t)mul;
    uint32_t bits;
    int32_t rows = 0;
    int m15 = 0, p16 = 0, k;

    if (lo == 0) {
        return exact;
    }
    bits = lo << 1; /* bit -1 = 0 */
    for (k = 0; k < 9; k++, bits >>= 2) {
        int d = (int)(bits & 1u) + (int)((bits >> 1) & 1u) - 2 * (int)((bits >> 2) & 1u);
        int32_t mag = (d < 0 ? -d : d) * mul;
        int32_t x = d < 0 ? ~mag : mag;
        int32_t high = x >> (16 - 2 * k);

        rows += high;
        if (k < 3) {
            m15 += (int)((x >> (15 - 2 * k)) & 1);
        }
        if (k >= 6) {
            p16 ^= (int)(high & 1);
        }
    }
    if (m15 >= 2) {
        rows++;
    }
    if (p16) {
        rows--;
    }
    return exact + (int64_t)rows * 65536;
}

/* sh4_fpu__sm(P, u), to 32 bits: every caller's result fits */
static inline int32_t sh4_fast_sm(int64_t p, int u)
{
    return p >= 0 ? (int32_t)(p >> u) : -(int32_t)((~p) >> u);
}

/* sh4_fsrra_internal: W on the 2^-26 grid, in [2^25, 2^26] */
static inline int32_t sh4_fast_fsrra_internal(uint32_t mantissa23, int parity)
{
    const int64_t K = (int64_t)1 << 19;
    int32_t m = (int32_t)(mantissa23 & 0x7fffffu);
    int32_t node = (m + (1 << 18)) >> 19;
    int32_t t = m - node * (1 << 19);
    const sh4_fsrra_word_t *w = &sh4_fsrra_rom[(parity ? 17 : 0) + node];
    int32_t dx = t * 16;
    int32_t sq = (int32_t)((sh4_fast_arr(dx, dx) + K) >> 23);
    int32_t t1 = sh4_fast_sm(sh4_fast_arr((int32_t)w->rec1, dx) + K, 26 - w->e1);
    int32_t t2 = sh4_fast_sm(sh4_fast_arr((int32_t)w->c2f * 512, sq) + K, 29);
    int32_t in = (int32_t)((sh4_fast_arr((int32_t)w->c3f * 256, sq) + K) >> 23);
    int32_t t3 = sh4_fast_sm(sh4_fast_arr(dx, in) + K, 27);

    return (int32_t)w->c0 - t1 + t2 - t3;
}

/* sh4_fsrra(x, 1) */
static inline uint32_t sh4_fast_fsrra(uint32_t x)
{
    uint32_t sign = x >> 31, exp = (x >> 23) & 0xffu, man = x & 0x7fffffu;
    int e, parity, k, top;
    uint32_t w;

    if (exp == 0xffu) {
        return (man || sign) ? 0x7fbfffffu : 0x00000000u; /* NaN, -inf: qNaN; +inf: +0 */
    }
    if (exp == 0) {
        return (sign << 31) | 0x7f800000u; /* +-0, denormals: +-inf */
    }
    if (sign) {
        return 0x7fbfffffu;
    }
    e = (int)exp - 127;
    parity = e & 1;
    k = (e - parity) / 2;
    w = (uint32_t)sh4_fast_fsrra_internal(man, parity);
    top = (w >> 26) ? 26 : 25;
    return ((uint32_t)(top - 26 - k + 127) << 23) | ((w >> (top - 23)) & 0x7fffffu);
}

#endif
