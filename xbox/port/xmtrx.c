/* the port's XMTRX (xbox/include/sh4.h), which 1ST_READ.BIN holds for every
 * image, and machine.h's st_ext, the store of it into m, row by row as the
 * console's pair stores leave it. SH4ZAM's software back end keeps an XMTRX
 * of its own, where shz_xmtrx_state_ says, but the port runs none of its
 * math (xbox/include/zam_xbox.h), so nothing defines that: a use of it
 * would be one of the link's missing names */
#include <machine.h>

#include "xbox/include/sh4.h"

Float sh4_xmtrx[16] __attribute__((aligned(64)));

void _builtin_st_ext(float (*m)[4])
{
    sh4_xmtrx_get((Float *)m);
}
