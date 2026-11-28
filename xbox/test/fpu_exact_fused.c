/* a fused product as the game's R26 and R32 objects compile one, for
 * xbox/test/fpu_exact.c: fpu_exact.sh contracts it in LLVM IR and runs
 * tools/pcfmac.py on it, as pc.mk's pc_compile_i does, so that it takes
 * the inline FMAC. noinline, so that it stays a function of its own */
__attribute__((noinline)) float test_fused(float a, float b, float c)
{
    return a * b + c;
}
