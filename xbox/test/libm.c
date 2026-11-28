/* sinf, cosf, sqrtf and fabsf for both of the harness's runs, so that the
 * tests' models of FSCA and FSRRA give the same bits on the SH-4 and on x86:
 * the same C and the same rounding, where each side's C library would
 * part. sinf and cosf need only be deterministic, not accurate to the bit */
float fabsf(float x)
{
    return x < 0.0F ? -x : x;
}

float sqrtf(float x)
{
    return __builtin_sqrtf(x);
}

/* x reduced to -pi to pi, then a Taylor series to x^15 */
float sinf(float x)
{
    const float tau = 6.28318530717958647692F;
    float term;
    float sum;
    int k;

    while (x > 3.14159265358979323846F) {
        x -= tau;
    }
    while (x < -3.14159265358979323846F) {
        x += tau;
    }
    term = x;
    sum = x;
    for (k = 1; k < 8; k++) {
        term *= -x * x / (float)((2 * k) * (2 * k + 1));
        sum += term;
    }
    return sum;
}

float cosf(float x)
{
    return sinf(x + 1.57079632679489661923F);
}

/* the tests' model of FSCA for both runs: the sine and cosine of a 16-bit
 * angle, 65536 to a turn, by sinf and cosf above. emulate.c runs it for the
 * SH-4's FSCA, and port_sincos16 below for the port's */
struct sh4_sincos {
    float sin, cos;
};

struct sh4_sincos test_sincos16(unsigned short angle)
{
    float radians = (float)angle / 65536.0F * 6.28318530718F;
    struct sh4_sincos pair;

    pair.sin = sinf(radians);
    pair.cos = cosf(radians);
    return pair;
}

/* the console's FSRRA for both runs, as the game takes it: on the x86 run,
 * xbox/port/fpu.c's, which xbox/include/sh4.h's sh4_fsrra calls, and on the
 * SH-4 run the vendored model it copies, which emulate.c runs for the
 * SH-4's FSRRA. xbox/test/fpu_exact.sh checks the two give the same bits */
#ifdef XBOX
#include "xbox/port/fpu.c"
#else
#include "../third_party/sh4_fpu_approx/sh4_fpu_approx.h"
#endif

float test_fsrra(float x)
{
    union {
        float f;
        uint32_t u;
    } bits;

    bits.f = x;
    bits.u = sh4_fsrra(bits.u, 1);
    return bits.f;
}

/* the port's side of the x86 run (xbox/include/sh4.h): its XMTRX, which
 * xmtrx.c loads and stores, FIPR's and FTRV's float sums (sh4_test_sums 1),
 * as emulate.c runs them on the SH-4 side, and the FSCA model above. the
 * game takes Flycast's sums (sh4_test_sums 0) and the console's FSCA
 * (xbox/port/math.c) */
float sh4_xmtrx[16];

int sh4_test_sums = 1;

struct sh4_sincos port_sincos16(unsigned short angle)
{
    return test_sincos16(angle);
}
