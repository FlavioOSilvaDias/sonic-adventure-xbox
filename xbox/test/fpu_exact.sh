#!/bin/sh
# builds and runs xbox/test/fpu_exact.c: the port's SH-4 FPU models
# (xbox/port/fpu.c, and tools/pcfmac.py's inline FMAC) against the console's
# bit for bit. the port's side, sh4.h's FSRRA and math.c's FSCA, compiles
# with xbox/test/run.sh's x86 flags, as the game compiles
set -e
cd "$(dirname "$0")/../.."
cc=${PC_CC:-clang}
out=build/pc/test/fpu_exact
mkdir -p "$out"
resource=$($cc -print-resource-dir)/include
flags="--target=i386-linux-gnu -march=pentium3 -msse -mfpmath=sse -std=gnu17 -ffreestanding -fno-builtin -nostdinc
    -isystem $resource -DNONMATCHING -DXBOX -DNDEBUG -D__CLANG_MAX_ALIGN_T_DEFINED
    -U__clang__ -fgnuc-version=4.2.1 -fpack-struct=4 -fno-strict-aliasing -fwrapv -frounding-math -fno-math-errno
    -ffp-contract=off -w -O2 -Isdk/include -Isrc -iquote ."
objects=
for source in xbox/test/fpu_exact_port.c xbox/port/math.c xbox/port/fpu.c; do
    object=$out/$(echo "$source" | tr / _).o
    # -Ddouble=float, as pc.mk's pc_double gives the port's C
    $cc $flags -Ddouble=float -c -o "$object" "$source"
    objects="$objects $object"
done
# a fused product as pc.mk's pc_compile_i compiles the game's: contracted in
# LLVM IR, then tools/pcfmac.py's inline FMAC, which must leave no call
fused=$out/fused
$cc $flags -Ddouble=float -ffp-contract=on -Xclang -disable-llvm-passes -S -emit-llvm -o "$fused.ll" \
    xbox/test/fpu_exact_fused.c
python3 tools/pcfmac.py "$fused.ll"
$cc $flags -Ddouble=float -c -o "$fused.o" "$fused.ll"
if nm "$fused.o" | grep -q 'port_fmac'; then
    echo "fpu_exact: the inline FMAC left a call in $fused.o" >&2
    exit 1
fi
objects="$objects $fused.o"
# play math's console build, the reference for FMAC and a second one for
# FSRRA and FSCA (engine/numeric/), as the C library's hosted C compiles it
$cc -m32 -march=pentium3 -msse -mfpmath=sse -O2 -frounding-math -ffp-contract=off -DSA_NUMERIC_CONSOLE \
    -Iengine/numeric -c -o "$out/play_math.o" engine/numeric/math.c
$cc -m32 -march=pentium3 -msse -mfpmath=sse -O2 -frounding-math -ffp-contract=off -iquote . \
    -o "$out/fpu_exact" xbox/test/fpu_exact.c $objects "$out/play_math.o"
"$out/fpu_exact"
