/* the helpers of sdk/include/ninja_zam.h that the Dreamcast engine's drawers
 * (engine/dc/) call, for the Xbox port. make dc's run the SH-4's FIPR, FSRRA
 * and FSCA through SH4ZAM's SH-4 back end. the port takes none of SH4ZAM: its
 * helpers run the port's models (sh4.h), on the vectors of vec_xbox.h, which
 * defines the few names of SH4ZAM's that the engine's shared C spells. sh4.h,
 * which ninja.h includes under XBOX where make dc's includes ninja_zam.h,
 * includes this after its models. ninja_zam.h's macros, which put its
 * helpers in place of Ninja's own calls, stay make dc's: the game's calls
 * keep the port's models of Ninja's bodies (xbox/sdk/ninja/) */
#ifndef XBOX_ZAM_XBOX_H
#define XBOX_ZAM_XBOX_H
#include "vec_xbox.h"

#include "sh4.h"

_Static_assert(sizeof(shz_vec3_t) == sizeof(NJS_POINT3), "shz_vec3_t stands for an NJS_POINT3");
_Static_assert(sizeof(shz_vec4_t) == 4 * sizeof(Float), "shz_vec4_t is four Floats");

/* p's x, y and z as a vector */
ALWAYS_INLINE shz_vec3_t zam_load_vec3(const NJS_POINT3 *p)
{
    return shz_vec3_init(p->x, p->y, p->z);
}

/* p's x, y and z, with w as the fourth component */
ALWAYS_INLINE shz_vec4_t zam_load_vec4(const NJS_POINT3 *p, Float w)
{
    return shz_vec4_init(p->x, p->y, p->z, w);
}

/* store v in p's x, y and z */
ALWAYS_INLINE void zam_store_vec3(NJS_POINT3 *p, shz_vec3_t v)
{
    p->x = v.x;
    p->y = v.y;
    p->z = v.z;
}

/* FIPR of a and b, a as FIPR's first operand */
ALWAYS_INLINE Float zam_fipr(shz_vec4_t a, shz_vec4_t b)
{
    return sh4_fipr(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
}

/* FIPR FV4,FV4: the sum of the squares of x, y, z and w */
ALWAYS_INLINE Float zam_fipr_self(Float x, Float y, Float z, Float w)
{
    return sh4_fipr_self(x, y, z, w);
}

/* the square of v's length: the FIPR of (v, 0) with itself */
ALWAYS_INLINE Float zam_squared_length(shz_vec3_t v)
{
    return zam_fipr_self(v.x, v.y, v.z, 0.0F);
}

/* FSCA's sine and cosine of ang */
ALWAYS_INLINE shz_sincos_t zam_fsca(Angle ang)
{
    Float sin;
    Float cos;
    shz_sincos_t pair;

    _builtin_fsca(ang, &sin, &cos);
    pair.sin = sin;
    pair.cos = cos;
    return pair;
}

/* the Dreamcast engine's two transforms that make dc runs through SH4ZAM,
 * which only draw (engine/dc/sa1/ is the drawer's, config/parts.txt): the
 * port's FTRV and FIPR for a drawer, as the native game draws on the host's
 * arithmetic */

/* FTRV XMTRX of (p, 1), the first three results: a sprite's place in the
 * view (engine/dc/sa1/sprite.c, sprite.h), as SH4ZAM's
 * shz_xmtrx_transform_point3 gives it on the SH-4 */
ALWAYS_INLINE shz_vec3_t zam_xmtrx_point3(shz_vec3_t p)
{
    Float v[4];

    v[0] = p.x;
    v[1] = p.y;
    v[2] = p.z;
    v[3] = 1.0F;
    sh4_xmtrx_ftrv_picture(v, v);
    return shz_vec3_init(v[0], v[1], v[2]);
}

/* v turned by XMTRX's transpose: the FIPR of (v, 0) with each of XMTRX's
 * first three columns, the column the operand the multiplier recodes, as
 * njModelE.c's shipped body turns the easy models' light (FIPR FV12,FV0,
 * FV12,FV4 and FV12,FV8; engine/dc/sa1/easy.h) */
ALWAYS_INLINE shz_vec3_t zam_xmtrx_turn3_transpose(shz_vec3_t v)
{
    const Float *c = sh4_xmtrx;

    return shz_vec3_init(sh4_fipr_picture(v.x, v.y, v.z, 0.0F, c[0], c[1], c[2], c[3]),
                         sh4_fipr_picture(v.x, v.y, v.z, 0.0F, c[4], c[5], c[6], c[7]),
                         sh4_fipr_picture(v.x, v.y, v.z, 0.0F, c[8], c[9], c[10], c[11]));
}

#endif
