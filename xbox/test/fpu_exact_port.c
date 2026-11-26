/* the port's side of xbox/test/fpu_exact.c: xbox/include/sh4.h's models of
 * FIPR, FTRV and FSRRA and xbox/port/math.c's FSCA, as the game compiles
 * them, each taking port_fpu's model (xbox/port/fpu.c), and the drawers'
 * models, behind plain calls that the test's hosted C can make. port_fmac
 * the test calls as it is. built with xbox/test/run.sh's x86 flags by
 * xbox/test/fpu_exact.sh */
#include "xbox/include/sh4.h"

float test_port_fipr(const float a[4], const float b[4])
{
    return sh4_fipr(a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
}

void test_port_ftrv(const float xf[16], const float v[4], float out[4])
{
    sh4_ftrv(xf, v, out);
}

float test_port_fsrra(float n)
{
    return _builtin_fsrra(n);
}

void test_port_fsca(unsigned angle, float *sin, float *cos)
{
    _builtin_fsca((Angle)angle, sin, cos);
}

/* the drawers' models (sh4.h's _picture ones), which take the port's own
 * whatever port_fpu says */
float test_port_fipr_picture(const float a[4], const float b[4])
{
    return sh4_fipr_picture(a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
}

void test_port_ftrv_picture(const float xf[16], const float v[4], float out[4])
{
    sh4_ftrv_picture(xf, v, out);
}

float test_port_fsrra_picture(float n)
{
    return sh4_fsrra_picture(n);
}
