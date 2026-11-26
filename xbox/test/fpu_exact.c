/* checks the port's SH-4 FPU models against the console's, bit for bit:
 * xbox/third_party/sh4_fpu_approx/'s models (MIT), measured on a Dreamcast,
 * and play math's FMAC (engine/numeric/math.c's sa_fmac, built for the
 * console), under the game's FPSCR (rounding toward zero, denormals as
 * zero), and under the modes the hosts set (MXCSR: rounding toward zero,
 * flush to zero, denormals as zero; x87: 53 bits, rounding toward zero).
 *
 * first, that each instruction under port_fpu 1, through sh4.h's and
 * math.c's dispatch (fpu_exact_port.c), gives the model's bits, that play
 * math's console models agree, and that the drawers' models stay the
 * port's own: those lines must say 0, and the run fails otherwise. then, for
 * port_fpu 0, how many of the port's own results differ from the console's,
 * and by how far; then each model's time a call, on this machine. run by
 * xbox/test/fpu_exact.sh */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbox/third_party/sh4_fpu_approx/sh4_fpu_approx.h"

extern int port_fpu[5];
float test_port_fipr(const float a[4], const float b[4]);
void test_port_ftrv(const float xf[16], const float v[4], float out[4]);
float test_port_fsrra(float n);
void test_port_fsca(unsigned angle, float *sin, float *cos);
float test_port_fipr_picture(const float a[4], const float b[4]);
void test_port_ftrv_picture(const float xf[16], const float v[4], float out[4]);
float test_port_fsrra_picture(float n);
float port_fmac(float f0, float fm, float fn);

/* play math's console build (engine/numeric/math.c, SA_NUMERIC_CONSOLE):
 * sa_fipr(a, b) recodes a, so it is FIPR FVb,FVa */
float sa_fipr(const float a[4], const float b[4]);
void sa_ftrv(const float m[16], const float v[4], float out[4]);
void sa_fsca(uint32_t angle, float out[2]);
float sa_fsrra(float a);
float sa_fmac(float f0, float fm, float fn);

enum { FIPR, FTRV, FSRRA, FSCA, FMAC };

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

static void say(const char *what, const struct tally *t, long of, int must_match)
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
    if (must_match && t->count != 0) {
        failed = 1;
    }
}

#define VECTORS 1000000L
#define MATRICES 200000L

static void check_fipr(int model)
{
    struct tally t = {0, 0, 0, 0, 0};
    long n;

    port_fpu[FIPR] = model;
    seed = 1;
    for (n = 0; n < VECTORS; n++) {
        float a[4], b[4];
        uint32_t am[4], bn[4];
        int k;

        for (k = 0; k < 4; k++) {
            a[k] = operand();
            b[k] = operand();
            am[k] = bits_of(a[k]);
            bn[k] = bits_of(b[k]);
        }
        tally(&t, bits_of(test_port_fipr(a, b)), sh4_fipr(am, bn, 1));
    }
    say(model ? "FIPR exact, through sh4.h" : "FIPR port (Flycast's x87 sums)", &t, VECTORS, model);
}

static void check_ftrv(int model, int in_place)
{
    struct tally t = {0, 0, 0, 0, 0};
    long n;

    port_fpu[FTRV] = model;
    seed = 2;
    for (n = 0; n < MATRICES; n++) {
        float xf[16], v[4], out[4];
        uint32_t xb[16], vb[4];
        int k;

        for (k = 0; k < 16; k++) {
            xf[k] = operand();
            xb[k] = bits_of(xf[k]);
        }
        for (k = 0; k < 4; k++) {
            v[k] = operand();
            vb[k] = bits_of(v[k]);
        }
        sh4_ftrv(xb, vb, 1);
        if (in_place) {
            test_port_ftrv(xf, v, v);
            memcpy(out, v, sizeof out);
        } else {
            test_port_ftrv(xf, v, out);
        }
        for (k = 0; k < 4; k++) {
            tally(&t, bits_of(out[k]), vb[k]);
        }
    }
    say(model ? (in_place ? "FTRV exact, through sh4.h, out = v" : "FTRV exact, through sh4.h")
              : "FTRV port (Flycast's x87 sums)",
        &t, MATRICES * 4, model);
}

static void check_fsrra(int model)
{
    static const uint32_t specials[] = {0x00000000u, 0x80000000u, 0x00000001u, 0x807fffffu, 0x7f800000u,
                                        0xff800000u, 0x7fc00000u, 0x7fa00000u, 0xbf800000u, 0x00800000u,
                                        0x7f7fffffu, 0x3f800000u, 0x40800000u, 0x3e800000u};
    struct tally t = {0, 0, 0, 0, 0}, s = {0, 0, 0, 0, 0};
    long n = 0, k;
    uint32_t x;

    port_fpu[FSRRA] = model;
    /* every input of [1, 4), whose results every positive normal's take
     * by the exponent alone, a step of 3 */
    for (x = 0x3f800000u; x < 0x40800000u; x += 3, n++) {
        tally(&t, bits_of(test_port_fsrra(float_of(x))), sh4_fsrra(x, 1));
    }
    say(model ? "FSRRA exact, through sh4.h, [1, 4)" : "FSRRA port (1 / sqrtf), [1, 4)", &t, n, model);
    if (model) {
        for (k = 0; k < (long)(sizeof specials / sizeof specials[0]); k++) {
            tally(&s, bits_of(test_port_fsrra(float_of(specials[k]))), sh4_fsrra(specials[k], 1));
        }
        say("FSRRA exact, through sh4.h, special operands", &s, k, 1);
    }
}

static void check_fsca(int model)
{
    struct tally t = {0, 0, 0, 0, 0};
    unsigned angle;

    port_fpu[FSCA] = model;
    for (angle = 0; angle < 0x10000u; angle++) {
        float sin, cos;
        uint32_t want_sin, want_cos;

        test_port_fsca(angle, &sin, &cos);
        sh4_fsca(angle, 1, &want_sin, &want_cos);
        tally(&t, bits_of(sin), want_sin);
        tally(&t, bits_of(cos), want_cos);
    }
    say(model ? "FSCA exact, through math.c" : "FSCA port (Flycast's table)", &t, 0x20000L, model);
}

/* the console's FMAC: port_fmac under port_fpu 1 against play math's, over
 * operands that cancel, that lie far apart, that are zeros or denormals */
static void check_fmac(void)
{
    struct tally t = {0, 0, 0, 0, 0}, apart = {0, 0, 0, 0, 0};
    long n;

    seed = 4;
    for (n = 0; n < VECTORS; n++) {
        float a = operand(), b = operand(), c;
        uint32_t r = next() % 4u;

        if (r == 0) {
            c = -(a * b); /* cancels, within a unit or so */
        } else if (r == 1) {
            c = float_of(bits_of(-(a * b)) ^ (next() & 0xffu));
        } else {
            c = operand();
        }
        port_fpu[FMAC] = 1;
        tally(&t, bits_of(port_fmac(a, b, c)), bits_of(sa_fmac(a, b, c)));
        port_fpu[FMAC] = 0;
        {
            volatile float product = a * b;
            volatile float sum = product + c;

            tally(&apart, bits_of(port_fmac(a, b, c)), bits_of(sum));
        }
    }
    say("FMAC exact, port_fmac against play math's", &t, VECTORS, 1);
    say("FMAC under HOST_FPU_FMAC=0: the product and sum apart", &apart, VECTORS, 1);
    port_fpu[FMAC] = 1;
}

/* how often the product and sum rounded apart, the port's build before it
 * fused them, part from the console's FMAC */
static void check_fmac_own(void)
{
    struct tally t = {0, 0, 0, 0, 0};
    long n;

    seed = 4;
    for (n = 0; n < VECTORS; n++) {
        float a = operand(), b = operand(), c = operand();
        volatile float product = a * b;
        volatile float sum = product + c;

        tally(&t, bits_of(sum), bits_of(sa_fmac(a, b, c)));
    }
    say("FMAC port (the product and sum apart)", &t, VECTORS, 0);
}

/* play math's console models (engine/numeric/) against the port's exact
 * ones, which take the same reference: a second reading of the operands */
static void check_play_math(void)
{
    struct tally t = {0, 0, 0, 0, 0}, v = {0, 0, 0, 0, 0}, r = {0, 0, 0, 0, 0}, c = {0, 0, 0, 0, 0};
    long n;
    unsigned angle;
    uint32_t x;

    port_fpu[FIPR] = port_fpu[FTRV] = port_fpu[FSRRA] = port_fpu[FSCA] = 1;
    seed = 5;
    for (n = 0; n < VECTORS / 4; n++) {
        float a[4], b[4], xf[16], vec[4], out[4], want[4];
        int k;

        for (k = 0; k < 4; k++) {
            a[k] = operand();
            b[k] = operand();
            vec[k] = operand();
        }
        for (k = 0; k < 16; k++) {
            xf[k] = operand();
        }
        tally(&t, bits_of(test_port_fipr(a, b)), bits_of(sa_fipr(b, a)));
        test_port_ftrv(xf, vec, out);
        sa_ftrv(xf, vec, want);
        for (k = 0; k < 4; k++) {
            tally(&v, bits_of(out[k]), bits_of(want[k]));
        }
    }
    for (x = 0x3f800000u; x < 0x40800000u; x += 7) {
        tally(&r, bits_of(test_port_fsrra(float_of(x))), bits_of(sa_fsrra(float_of(x))));
    }
    for (angle = 0; angle < 0x10000u; angle++) {
        float s, co, pair[2];

        test_port_fsca(angle, &s, &co);
        sa_fsca(angle, pair);
        tally(&c, bits_of(s), bits_of(pair[0]));
        tally(&c, bits_of(co), bits_of(pair[1]));
    }
    say("FIPR exact against play math's console FIPR", &t, VECTORS / 4, 1);
    say("FTRV exact against play math's console FTRV", &v, VECTORS, 1);
    say("FSRRA exact against play math's, [1, 4)", &r, (0x40800000L - 0x3f800000L) / 7, 1);
    say("FSCA exact against play math's", &c, 0x20000L, 1);
}

/* the drawers' models stay the port's own with play's all exact */
static void check_picture(void)
{
    struct tally t = {0, 0, 0, 0, 0}, v = {0, 0, 0, 0, 0}, r = {0, 0, 0, 0, 0};
    long n;

    seed = 6;
    for (n = 0; n < VECTORS / 4; n++) {
        float a[4], b[4], xf[16], vec[4], out[4], want[4], f;
        uint32_t own, drawn;
        int k;

        for (k = 0; k < 4; k++) {
            a[k] = operand();
            b[k] = operand();
            vec[k] = operand();
        }
        for (k = 0; k < 16; k++) {
            xf[k] = operand();
        }
        f = float_of(next() & 0x7fffffffu);
        port_fpu[FIPR] = port_fpu[FTRV] = port_fpu[FSRRA] = 0;
        own = bits_of(test_port_fipr(a, b));
        test_port_ftrv(xf, vec, want);
        {
            uint32_t root = bits_of(test_port_fsrra(f));

            port_fpu[FIPR] = port_fpu[FTRV] = port_fpu[FSRRA] = 1;
            tally(&r, bits_of(test_port_fsrra_picture(f)), root);
        }
        drawn = bits_of(test_port_fipr_picture(a, b));
        tally(&t, drawn, own);
        test_port_ftrv_picture(xf, vec, out);
        for (k = 0; k < 4; k++) {
            tally(&v, bits_of(out[k]), bits_of(want[k]));
        }
    }
    say("FIPR for a drawer, under play's exact: the port's", &t, VECTORS / 4, 1);
    say("FTRV for a drawer, under play's exact: the port's", &v, VECTORS, 1);
    say("FSRRA for a drawer, under play's exact: the port's", &r, VECTORS / 4, 1);
}

static unsigned long long cycles(void)
{
    return __builtin_ia32_rdtsc();
}

/* each model's cycles a call, over the same operands */
static void time_models(void)
{
    enum { CALLS = 200000 };
    static float a[CALLS][4], b[CALLS][4];
    volatile float sink;
    int model, k;

    seed = 3;
    for (k = 0; k < CALLS; k++) {
        int j;

        for (j = 0; j < 4; j++) {
            a[k][j] = operand();
            b[k][j] = operand();
        }
    }
    printf("\ncycles a call on this machine (the Xbox's Pentium III is slower in each, more so in 64-bit integer work):\n");
    for (model = 0; model < 2; model++) {
        unsigned long long start;
        double fipr, ftrv, fsrra, fsca, fmac;
        float out[4], s, c;

        port_fpu[FIPR] = port_fpu[FTRV] = port_fpu[FSRRA] = port_fpu[FSCA] = port_fpu[FMAC] = model;
        start = cycles();
        for (k = 0; k < CALLS; k++) {
            sink = test_port_fipr(a[k], b[k]);
        }
        fipr = (double)(cycles() - start) / CALLS;
        start = cycles();
        for (k = 0; k < CALLS / 4; k++) {
            test_port_ftrv(&a[4 * k][0], b[k], out);
        }
        ftrv = (double)(cycles() - start) / (CALLS / 4);
        start = cycles();
        for (k = 0; k < CALLS; k++) {
            sink = test_port_fsrra(a[k][0] < 0.0F ? -a[k][0] : a[k][0]);
        }
        fsrra = (double)(cycles() - start) / CALLS;
        start = cycles();
        for (k = 0; k < CALLS; k++) {
            test_port_fsca((unsigned)k * 40503u, &s, &c);
        }
        fsca = (double)(cycles() - start) / CALLS;
        start = cycles();
        for (k = 0; k < CALLS; k++) {
            sink = port_fmac(a[k][0], b[k][0], a[k][1]);
        }
        fmac = (double)(cycles() - start) / CALLS;
        sink = out[0] + s + c;
        printf("  %-6s FIPR %7.1f   FTRV %7.1f   FSRRA %7.1f   FSCA %7.1f   FMAC %7.1f\n", model ? "exact" : "port",
               fipr, ftrv, fsrra, fsca, fmac);
    }
    (void)sink;
    port_fpu[FIPR] = port_fpu[FTRV] = port_fpu[FSRRA] = port_fpu[FSCA] = port_fpu[FMAC] = 0;
}

int main(void)
{
    /* the hosts' fpu_as_console: MXCSR rounding toward zero, flush to zero,
     * denormals as zero; the x87 at 53 bits, rounding toward zero */
    unsigned csr = 0x1f80u | 0x6000u | 0x8000u | 0x0040u;
    unsigned short word = 0x0e7f;

    __asm__ volatile("ldmxcsr %0" : : "m"(csr));
    __asm__ volatile("fldcw %0" : : "m"(word));

    printf("the exact models through the port's dispatch, against the console's (must be 0):\n");
    check_fipr(1);
    check_ftrv(1, 0);
    check_ftrv(1, 1);
    check_fsrra(1);
    check_fsca(1);
    check_fmac();
    check_play_math();
    check_picture();
    printf("\nthe port's own models, against the console's:\n");
    check_fipr(0);
    check_ftrv(0, 0);
    check_fsrra(0);
    check_fsca(0);
    check_fmac_own();
    time_models();
    printf("\nfpu_exact: %s\n", failed ? "FAILED: the exact models' dispatch differs" : "0 real");
    return failed;
}
