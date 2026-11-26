/* the vectors of the Dreamcast engine's shared C (engine/dc/sa1/, njModel.c,
 * xbox/engine/), for the Xbox port, which takes none of SH4ZAM: make dc's
 * drawers spell their vectors by SH4ZAM's types, initializers and prefetch,
 * and this defines those few names alone, with their layout on x86, and no
 * SH4ZAM code. the math on them is the port's (xbox/include/sh4.h,
 * zam_xbox.h). a source that needs more of SH4ZAM's names fails to compile
 * on the Xbox, where no include path holds SH4ZAM's headers (config/pc.mk):
 * its math then wants a port's model, as the native game's takes play
 * math's */
#ifndef XBOX_VEC_XBOX_H
#define XBOX_VEC_XBOX_H

typedef struct shz_vec2 {
    union {
        float e[2];
        struct {
            float x, y;
        };
    };
} shz_vec2_t;

typedef struct shz_vec3 {
    union {
        float e[3];
        struct {
            union {
                struct {
                    float x, y;
                };
                shz_vec2_t xy;
            };
            float z;
        };
    };
} shz_vec3_t;

/* four floats as one 16-byte vector, so 16-byte aligned, as SH4ZAM lays it
 * out on a host with SIMD */
typedef struct shz_vec4 {
    union {
        float e __attribute__((vector_size(16)));
        struct {
            union {
                struct {
                    float x, y, z;
                };
                shz_vec3_t xyz;
            };
            float w;
        };
        struct {
            shz_vec2_t xy, zw;
        };
    };
} shz_vec4_t;

/* FSCA's pair: the sine, then the cosine */
typedef struct shz_sincos {
    float sin, cos;
} shz_sincos_t;

static inline __attribute__((always_inline)) shz_vec3_t shz_vec3_init(float x, float y, float z)
{
    shz_vec3_t v;

    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static inline __attribute__((always_inline)) shz_vec4_t shz_vec4_init(float x, float y, float z, float w)
{
    shz_vec4_t v;

    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

/* the SH-4's PREF, a hint to load the cache line at p, as x86's prefetch.
 * p may be an address as an integer, as the drawers pass one */
#define SHZ_PREFETCH(p) __builtin_prefetch((const void *)(p))

#endif
