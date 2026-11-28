/* the C runtime's atan2f, asinf and acosf (sdk/runtime/): SHC's code for
 * them on the SH-4, against the game's x86 objects on x86, as make pc builds
 * them, their fused products the console's FMAC (tools/pcfmac.py). the
 * game's turns and slopes take them every step, so a difference in a last
 * bit moves play */
#include "harness.h"

float atan2f(float y, float x);
float asinf(float x);
float acosf(float x);

#ifdef __sh__
/* the runtime sets it on a domain error: _errno in its assembly */
int _errno;
#else
int _errno;
#endif

#define CASES 200000

/* a float from -range to range, mostly the game's sizes, some tiny or huge */
static float pick(float range)
{
    unsigned r = harness_random();

    switch (r >> 28) {
    case 0:
        return harness_value(1.0e-6F);
    case 1:
        return harness_value(1.0e6F);
    case 2: {
        union {
            unsigned u;
            float f;
        } bits;

        bits.u = harness_random();
        /* no denormal: SHC's code reads one as 0, as the FPSCR's DN asks,
         * where x86 returns it as it is from asinf and branches otherwise in
         * atan2f. both machines flush every tiny result to 0, so none reaches
         * these in play */
        if ((bits.u & 0x7f800000u) == 0) {
            bits.u &= 0x80000000u;
        }
        return bits.f;
    }
    default:
        return harness_value(range);
    }
}

int main(void)
{
    unsigned k;

    harness_fpu();
#ifndef __sh__
    {
        /* the x87's mode for the FMAC's double: 53 bits, toward zero */
        unsigned short word = 0x0e7f;

        __asm__ volatile("fldcw %0" : : "m"(word));
    }
#endif
    for (k = 0; k < CASES; k++) {
        float y = pick(1000.0F), x = pick(1000.0F), s = pick(1.0F);

        harness_begin("atan2f", k);
        harness_float(y);
        harness_float(x);
        harness_float(atan2f(y, x));
        harness_end();
        harness_begin("asinf", k);
        harness_float(s);
        harness_float(asinf(s));
        harness_end();
        harness_begin("acosf", k);
        harness_float(s);
        harness_float(acosf(s));
        harness_end();
    }
    return 0;
}
