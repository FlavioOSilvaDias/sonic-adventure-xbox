/* the C library's float sine and cosine, which make pc's objects call and
 * no library supplies: x87's FSIN and FCOS, which compute at the x87's own
 * precision on the float loaded to the stack's top, exact to a float's for
 * the angles the game passes, well inside their 2^63 range */
float sinf(float x)
{
    float r = x;

    __asm__("fsin" : "+t"(r));
    return r;
}

float cosf(float x)
{
    float r = x;

    __asm__("fcos" : "+t"(r));
    return r;
}

#include "xbox/include/sh4.h"

/* sh4_sse.h's FIPR and FTRV take Flycast's sums where this is 0, as the
 * game does; xbox/test/libm.c sets it for the tests' float sums */
int sh4_test_sums;

/* FSCA's sine and cosine of a 16-bit angle, 65536 to a turn, as the SH-4
 * gives them under the game's rounding toward zero: the exact model's
 * (xbox/port/fpu.c) */
struct sh4_sincos port_sincos16(unsigned short angle)
{
    struct sh4_sincos pair;

    port_fsca_exact(angle, &pair.sin, &pair.cos);
    return pair;
}
