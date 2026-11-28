/* the port's side of xbox/test/fpu_exact.c: xbox/include/sh4.h's FSRRA and
 * xbox/port/math.c's FSCA, as the game compiles them, behind plain calls
 * that the test's hosted C can make. port_fmac the test calls as it is.
 * built with xbox/test/run.sh's x86 flags by xbox/test/fpu_exact.sh */
#include "xbox/include/sh4.h"

float test_port_fsrra(float n)
{
    return _builtin_fsrra(n);
}

void test_port_fsca(unsigned angle, float *sin, float *cos)
{
    _builtin_fsca((Angle)angle, sin, cos);
}
