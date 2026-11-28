/* the SH-4's FSRRA and FSCA as the console's FPU computes them, bit for
 * bit: xbox/third_party/sh4_fpu_approx/'s models (MIT), measured against a
 * Dreamcast, through the port's 32-bit copy of FSRRA's (sh4_fpu_fast.h) and
 * a table of the model's sines for FSCA (fsca_exact_table.h), and its FMAC.
 * xbox/include/sh4.h's models take them. the console's FPSCR is the game's
 * whole run: rounding toward zero, denormals as zero (DN 1), so the models'
 * plain functions, which give DN 1's values and take no trap, are the ones.
 *
 * FSRRA's and FSCA's models work on integers alone, and need no FPU mode.
 * this includes no port header, so that xbox/test/libm.c can take it for
 * the tests' x86 side */
#include "xbox/port/sh4_fpu_fast.h"
/* pc.mk's pc_double compiles the port's C with -Ddouble=float, as SHC's
 * -fpu=single has the game's; port_fmac needs a true double for its one
 * rounding, which a float product and sum would make two */
#undef double
union fpu_bits {
    float f;
    uint32_t u;
};

float port_fsrra_exact(float n)
{
    union fpu_bits bits;

    bits.f = n;
    bits.u = sh4_fast_fsrra(bits.u);
    return bits.f;
}

/* the model's sines (sh4_fsca_sin under rounding toward zero) for angles 0
 * to 0x7fff: xbox/test/fsca_table.c writes them */
#include "xbox/port/fsca_exact_table.h"

/* the model's sine of angle: the second half turn's is the first's with
 * its sign flipped, a zero staying +0 (fsca_table.c) */
static uint32_t fsca_exact_sine(unsigned angle)
{
    uint32_t u = fsca_exact_sines[angle & 0x7fffu];

    return (angle & 0x8000u) && u != 0 ? u ^ 0x80000000u : u;
}

/* FSCA: FPUL's low 16 bits, 65536 to a turn. the cosine is the sine a
 * quarter turn on, as sh4_fsca_cos takes it */
void port_fsca_exact(unsigned angle, float *sin, float *cos)
{
    union fpu_bits s, c;

    s.u = fsca_exact_sine(angle);
    c.u = fsca_exact_sine(angle + 0x4000u);
    *sin = s.f;
    *cos = c.f;
}

/* a float's bits with a denormal as its signed zero, as the FPSCR's DN 1
 * takes one */
static uint32_t fpu_flushed(uint32_t u)
{
    return (u & 0x7f800000u) == 0 ? u & 0x80000000u : u;
}

/* FMAC FR0,FRm,FRn as the console gives it under the game's FPSCR, which
 * play math's sa_fmac models (engine/numeric/math.c): f0 * fm + fn rounded
 * once toward zero, a denormal operand as its signed zero, and a result below
 * the normal range as its signed zero. a float product is exact as a double,
 * and the sum, which the x87 rounds toward zero to a double's 53 bits (the
 * hosts' control word 0x0e7f) and the conversion again to a float's 24,
 * truncates as one rounding toward zero does. zeros sum as IEEE's do toward
 * zero, as sa_fmac's: -0 only where both are -0. the Xbox's Pentium III may
 * lack SSE's denormals-as-zero, so the operands' flush is by hand. the
 * game's fused products don't call this: tools/pcfmac.py writes the same
 * inline, in LLVM IR, at each, which xbox/test/fpu_exact.sh checks against
 * this */
float port_fmac(float f0, float fm, float fn)
{
    union fpu_bits a, b, c, r;

    a.f = f0;
    b.f = fm;
    c.f = fn;
    a.u = fpu_flushed(a.u);
    b.u = fpu_flushed(b.u);
    c.u = fpu_flushed(c.u);
    r.f = (float)((double)a.f * (double)b.f + (double)c.f);
    r.u = fpu_flushed(r.u);
    return r.f;
}
