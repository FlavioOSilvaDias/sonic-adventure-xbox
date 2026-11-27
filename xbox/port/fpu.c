/* the SH-4's FIPR, FTRV, FSRRA and FSCA as the console's FPU computes them,
 * bit for bit: xbox/third_party/sh4_fpu_approx/'s models (MIT), measured
 * against a Dreamcast. xbox/include/sh4.h's models take them, one
 * instruction at a time, where port_fpu says so; else they keep the port's
 * own (Flycast's x87 sums for FIPR and FTRV, an exact 1 / sqrt for FSRRA,
 * the console's table for FSCA). the console's FPSCR is the game's whole
 * run: rounding toward zero, denormals as zero (DN 1), so the models' plain
 * functions, which give DN 1's values and take no trap, are the ones.
 *
 * the models work on integers alone, so this file needs no FPU mode. it
 * includes no port header, so that xbox/test/libm.c can take it for the
 * tests' x86 side */
#include "xbox/third_party/sh4_fpu_approx/sh4_fpu_approx.h"
/* pc.mk's pc_double compiles the port's C with -Ddouble=float, as SHC's
 * -fpu=single has the game's; port_fmac's exact path needs a true double
 * for its one rounding, which a float product and sum would make two */
#undef double

/* FPSCR.RM 1: rounding toward zero, the game's mode */
#define FPU_RZ 1

/* each instruction's model in play, xbox/include/sh4.h's SH4_FPU_ order: 0
 * the port's, 1 the exact one. the host sets each from HOST_FPU_FIPR,
 * HOST_FPU_FTRV, HOST_FPU_FSRRA, HOST_FPU_FSCA and HOST_FPU_FMAC before main
 * (host.h's host_fpu_defaults where a setting is missing), and flips them on
 * the pad's chord (host.h's host_fpu_flip). 0 here, where no host sets
 * them, as for xbox/test/'s x86 side */
int port_fpu[5];
/* the frames left of the models' names on the screen after a flip
 * (port_fpu_draw in port.c), which the host sets */
volatile int port_fpu_note;
/* nonzero where the port's own models are asked for, whatever port_fpu
 * says: sound's math and Ninja's lights (xbox/include/sh4.h's
 * SH4_FPU_OWN_BEGIN) */
int port_fpu_own;
/* 1 by a poke: every model port_fpu's, sound's, the lights' and the
 * drawers' too (xbox/include/sh4.h) */
int port_fpu_all;

union fpu_bits {
    float f;
    uint32_t u;
};

/* FIPR FVm,FVn: a the FVm, b the FVn, which the multiplier recodes, as
 * tools/sh2c.py passes them to sh4_fipr */
float port_fipr_exact(float a0, float a1, float a2, float a3, float b0, float b1, float b2, float b3)
{
    union fpu_bits m[4], n[4], r;
    uint32_t mu[4], nu[4];
    int k;

    m[0].f = a0, m[1].f = a1, m[2].f = a2, m[3].f = a3;
    n[0].f = b0, n[1].f = b1, n[2].f = b2, n[3].f = b3;
    for (k = 0; k < 4; k++) {
        mu[k] = m[k].u;
        nu[k] = n[k].u;
    }
    r.u = sh4_fipr(mu, nu, FPU_RZ);
    return r.f;
}

/* FTRV of v by the matrix whose XF0 to XF15 are xf: v and xf are read
 * whole before out is written, so out may be v, or a column of xf */
void port_ftrv_exact(const float xf[16], const float v[4], float out[4])
{
    union fpu_bits bits;
    uint32_t matrix[16], vector[4];
    int k;

    for (k = 0; k < 16; k++) {
        bits.f = xf[k];
        matrix[k] = bits.u;
    }
    for (k = 0; k < 4; k++) {
        bits.f = v[k];
        vector[k] = bits.u;
    }
    sh4_ftrv(matrix, vector, FPU_RZ);
    for (k = 0; k < 4; k++) {
        bits.u = vector[k];
        out[k] = bits.f;
    }
}

float port_fsrra_exact(float n)
{
    union fpu_bits bits;

    bits.f = n;
    bits.u = sh4_fsrra(bits.u, FPU_RZ);
    return bits.f;
}

/* FSCA: FPUL's low 16 bits, 65536 to a turn */
void port_fsca_exact(unsigned angle, float *sin, float *cos)
{
    union fpu_bits s, c;

    sh4_fsca(angle & 0xffffu, FPU_RZ, &s.u, &c.u);
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
 * lack SSE's denormals-as-zero, so the operands' flush is by hand. under
 * HOST_FPU_FMAC=0, the product and the sum round apart, in SSE, as the
 * port's build gave them before it fused them (tools/pcfmac.py) */
float port_fmac(float f0, float fm, float fn)
{
    union fpu_bits a, b, c, r;

    if (!port_fpu[4] || (port_fpu_own && !port_fpu_all)) {
        return f0 * fm + fn;
    }
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
