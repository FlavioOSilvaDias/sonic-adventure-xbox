/* checks the port's SH-4 FPU models against the console's, bit for bit:
 * xbox/third_party/sh4_fpu_approx/'s models (MIT), measured on a Dreamcast,
 * and play math's console build (engine/numeric/math.c, SA_NUMERIC_CONSOLE),
 * under the game's FPSCR (rounding toward zero, denormals as zero), and
 * under the modes the hosts set (MXCSR: rounding toward zero, flush to
 * zero, denormals as zero; x87: 53 bits, rounding toward zero): FSRRA
 * through sh4.h, FSCA through math.c (fpu_exact_port.c), FMAC as
 * port_fmac and as tools/pcfmac.py's inline one, and the port's 32-bit copy
 * of FSRRA's model (sh4_fpu_fast.h) against the vendored one. each line
 * must say 0, and the run fails otherwise. run by xbox/test/fpu_exact.sh */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbox/third_party/sh4_fpu_approx/sh4_fpu_approx.h"
#include "xbox/port/sh4_fpu_fast.h"

float test_port_fsrra(float n);
void test_port_fsca(unsigned angle, float *sin, float *cos);
float port_fmac(float f0, float fm, float fn);
/* a + b * c as the game's fused products compile: through tools/pcfmac.py's
 * inline FMAC (fpu_exact_fused.c, which fpu_exact.sh builds as pc.mk does) */
float test_fused(float a, float b, float c);

/* play math's console build */
void sa_fsca(uint32_t angle, float out[2]);
float sa_fsrra(float a);
float sa_fmac(float f0, float fm, float fn);

static uint32_t bits_of(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof u);
    return u;
}

static float float_of(uint32_t u)
{
    float f;

    memcpy(&f, &u, sizeof f);
    return f;
}

static uint32_t seed = 0x5eed1234u;

static uint32_t next(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

/* a game's operand: either sign, magnitudes from 2^-12 to 2^12, with a
 * zero, a one, an exact small integer or a denormal now and then */
static float operand(void)
{
    uint32_t r = next();
    uint32_t sign = (r & 1u) << 31;

    switch ((r >> 1) & 15u) {
    case 0:
        return float_of(sign);
    case 1:
        return float_of(sign | 0x3f800000u);
    case 2:
        return float_of(sign | (next() & 0x7fffffu)); /* a denormal */
    case 3:
        return (float)(int)((r >> 8) % 64u) * (sign ? -1.0F : 1.0F);
    default:
        return float_of(sign | (uint32_t)(127 - 12 + (int)((r >> 8) % 25u)) << 23 | (next() & 0x7fffffu));
    }
}

/* the differences found: how many, how many by the sign alone (a zero's),
 * how many by one unit of the last place, and the most units among the
 * rest. a NaN for a NaN is no difference: the console's quiet NaN,
 * 0x7fbfffff, is a signalling one to x86, which quiets it as a float
 * returns on the x87, as every NaN of the port's is x86's */
struct tally {
    long count, signs, ones, most, nans;
};

static int is_nan(uint32_t u)
{
    return (u & 0x7f800000u) == 0x7f800000u && (u & 0x7fffffu) != 0;
}

static void tally(struct tally *t, uint32_t got, uint32_t want)
{
    long units;

    if (got == want) {
        return;
    }
    if (is_nan(got) && is_nan(want)) {
        t->nans++;
        return;
    }
    t->count++;
    if ((got ^ want) >> 31) {
        t->signs++;
        return;
    }
    units = (long)(got & 0x7fffffffu) - (long)(want & 0x7fffffffu);
    units = units < 0 ? -units : units;
    if (units == 1) {
        t->ones++;
    }
    if (units > t->most) {
        t->most = units;
    }
}

static int failed;

static void say(const char *what, const struct tally *t, long of)
{
    printf("%-44s %8ld of %9ld differ", what, t->count, of);
    if (t->count != 0) {
        printf(" (%.2f%%; %ld by 1 ulp, at most %ld", 100.0 * (double)t->count / (double)of, t->ones, t->most);
        if (t->signs != 0) {
            printf(", %ld by a zero's sign", t->signs);
        }
        printf(")");
    }
    if (t->nans != 0) {
        printf("; %ld NaNs for NaNs", t->nans);
    }
    printf("\n");
    if (t->count != 0) {
        failed = 1;
    }
}

#define VECTORS 1000000L

static void check_fsrra(void)
{
    static const uint32_t specials[] = {0x00000000u, 0x80000000u, 0x00000001u, 0x807fffffu, 0x7f800000u,
                                        0xff800000u, 0x7fc00000u, 0x7fa00000u, 0xbf800000u, 0x00800000u,
                                        0x7f7fffffu, 0x3f800000u, 0x40800000u, 0x3e800000u};
    struct tally t = {0, 0, 0, 0, 0}, s = {0, 0, 0, 0, 0}, p = {0, 0, 0, 0, 0};
    long n = 0, k;
    uint32_t x;

    /* every input of [1, 4), whose results every positive normal's take
     * by the exponent alone, a step of 3 */
    for (x = 0x3f800000u; x < 0x40800000u; x += 3, n++) {
        tally(&t, bits_of(test_port_fsrra(float_of(x))), sh4_fsrra(x, 1));
        tally(&p, bits_of(test_port_fsrra(float_of(x))), bits_of(sa_fsrra(float_of(x))));
    }
    say("FSRRA through sh4.h, [1, 4)", &t, n);
    for (k = 0; k < (long)(sizeof specials / sizeof specials[0]); k++) {
        tally(&s, bits_of(test_port_fsrra(float_of(specials[k]))), sh4_fsrra(specials[k], 1));
    }
    say("FSRRA through sh4.h, special operands", &s, k);
    say("FSRRA against play math's, [1, 4)", &p, n);
}

static void check_fsca(void)
{
    struct tally t = {0, 0, 0, 0, 0}, p = {0, 0, 0, 0, 0};
    unsigned angle;

    for (angle = 0; angle < 0x10000u; angle++) {
        float sin, cos, pair[2];
        uint32_t want_sin, want_cos;

        test_port_fsca(angle, &sin, &cos);
        sh4_fsca(angle, 1, &want_sin, &want_cos);
        tally(&t, bits_of(sin), want_sin);
        tally(&t, bits_of(cos), want_cos);
        sa_fsca(angle, pair);
        tally(&p, bits_of(sin), bits_of(pair[0]));
        tally(&p, bits_of(cos), bits_of(pair[1]));
    }
    say("FSCA through math.c", &t, 0x20000L);
    say("FSCA against play math's", &p, 0x20000L);
}

/* the console's FMAC, port_fmac and the fused products as the game compiles
 * them, tools/pcfmac.py's inline FMAC, against play math's, over operands
 * that cancel, that lie far apart, that are zeros or denormals */
static void check_fmac(void)
{
    struct tally t = {0, 0, 0, 0, 0}, inline_ = {0, 0, 0, 0, 0};
    long n;

    seed = 4;
    for (n = 0; n < VECTORS; n++) {
        float a = operand(), b = operand(), c;
        uint32_t r = next() % 4u;
        uint32_t want;

        if (r == 0) {
            c = -(a * b); /* cancels, within a unit or so */
        } else if (r == 1) {
            c = float_of(bits_of(-(a * b)) ^ (next() & 0xffu));
        } else {
            c = operand();
        }
        want = bits_of(sa_fmac(a, b, c));
        tally(&t, bits_of(port_fmac(a, b, c)), want);
        tally(&inline_, bits_of(test_fused(a, b, c)), want);
    }
    say("FMAC, port_fmac against play math's", &t, VECTORS);
    say("FMAC, inline (pcfmac.py), against play math's", &inline_, VECTORS);
}

/* the port's copy (sh4_fpu_fast.h) against the vendored model directly: the
 * multiplier array over operands of every width FSRRA gives it, and FSRRA
 * over every input of [1, 4), all 2^23 mantissas of both exponent parities,
 * which with the exponent's rule are every positive normal's results, and
 * random inputs of every exponent */
static void check_fast(void)
{
    struct tally arr = {0, 0, 0, 0, 0}, root = {0, 0, 0, 0, 0}, any = {0, 0, 0, 0, 0};
    long n, arrays = 0;
    uint32_t x;

    seed = 8;
    for (n = 0; n < 20000000L; n++) {
        static const int widths[] = {28, 25, 24, 23, 22, 20, 14, 8};
        uint32_t r = next();
        int rw = widths[r % 8u], mw = widths[1 + (r >> 3) % 7u];
        /* signed values of rw and mw bits, rec's low 18 bits zero now and then */
        int32_t rec = (int32_t)(next() << (32 - rw)) >> (32 - rw);
        int32_t mul = (int32_t)(next() << (32 - mw)) >> (32 - mw);

        if ((r >> 6) % 16u == 0) {
            rec &= ~0x3ffff;
        }
        if (sh4_fpu__arr(rec, mul) != sh4_fast_arr(rec, mul)) {
            arr.count++;
        }
        arrays++;
    }
    for (x = 0x3f800000u; x < 0x40800000u; x++) {
        tally(&root, sh4_fast_fsrra(x), sh4_fsrra(x, 1));
    }
    for (n = 0; n < 1000000L; n++) {
        x = next();
        tally(&any, sh4_fast_fsrra(x), sh4_fsrra(x, 1));
    }
    say("the multiplier array, the port's copy", &arr, arrays);
    say("FSRRA, the port's copy, every input of [1, 4)", &root, 0x1000000L);
    say("FSRRA, the port's copy, any input", &any, 1000000L);
}

int main(void)
{
    /* the hosts' fpu_as_console: MXCSR rounding toward zero, flush to zero,
     * denormals as zero; the x87 at 53 bits, rounding toward zero */
    unsigned csr = 0x1f80u | 0x6000u | 0x8000u | 0x0040u;
    unsigned short word = 0x0e7f;

    __asm__ volatile("ldmxcsr %0" : : "m"(csr));
    __asm__ volatile("fldcw %0" : : "m"(word));

    printf("the port's models against the console's (must be 0):\n");
    check_fsrra();
    check_fsca();
    check_fmac();
    check_fast();
    printf("\nfpu_exact: %s\n", failed ? "FAILED: a model differs from the console's" : "0 real");
    return failed;
}
