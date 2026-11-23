#!/bin/sh
# run every translation test of xbox/test/, each by run.sh with its
# assembly and its C, and fail where any shows a real difference. Run it
# from the checkout's root after make xbox (or make dc and make
# xbox-pc-test), which leaves make dc's split C in build/dc/ and make pc's
# in build/pc/.
#
# the SH-4 side is the shipped assembly. make dc builds some of it from
# SH4ZAM's C instead (config/dreamcast.mk's DC_REPLACED: math.src, mesh.src,
# Ninja's matrix and vector bodies), so this makes each body's assembly
# itself, under build/pc/test/orig/, as make dc translates one it keeps:
# a .src by build/shckit, a Ninja source's inline bodies split from its
# shipped side (shc_side.py) by tools/inline.py, then build/shckit
set -u
o=build/pc/test/orig x=xbox/sdk/ninja m=xbox/test/xmtrx.c b=build/pc/test/orig/sdk/ninja d=build/dc
status=0
mkdir -p build/pc/test $b
run() {
    xbox/test/run.sh "$@" > build/pc/test/run.log 2>&1
    line=$(grep 'lines;' build/pc/test/run.log)
    echo "${line:-$1: failed to run, as build/pc/test/run.log shows}"
    case $line in
    *" 0 real,"*) ;;
    *) status=1 ;;
    esac
}
# $o/PATH.s, the GNU as text of the shipped PATH.src
src_asm() {
    mkdir -p "$o/$(dirname "$1")"
    python3 tools/floats.py --assembly "$1.src" && build/shckit translate -o "$o/$1.s" "$1.src"
}
# $b/NAME_inline.s, the GNU as text of sdk/ninja/NAME.c's shipped inline bodies
ninja_asm() {
    python3 xbox/test/shc_side.py "sdk/ninja/$1.c" > "$b/$1.c" && \
        python3 tools/inline.py "$b/$1.c" "$b/$1_split.c" "$b/$1_inline.src" && \
        python3 tools/floats.py --assembly "$b/$1_inline.src" && \
        build/shckit translate -o "$b/$1_inline.s" "$b/$1_inline.src"
}
for f in src/system/math src/system/vector src/collision/mesh src/draw/palette modules/adv0100/palette \
    src/system/crt0 src/system/cache; do
    src_asm $f
done
for f in njAddVector njSubVector njOuterProduct njFraction njRoundOff njGetMatrix njUnitMatrix njSetMatrix \
    njInitMatrix njRotateX njRotateY njRotateZ njRotateXYZ njRotateZXY njRotate njSetPerspective \
    njRotateViewXAbsolute njRotateViewXRelative njRotateViewYRelative njRotateViewZAbsolute njRotateViewZRelative \
    njTranslateViewAbsolute njTranslateViewRelative njCalcVector njClipZ njProjectScreen njMultiMatrix njMirror \
    njSetView njInitView njInvertMatrix njDetMatrix njClearMatrix njSetLightAngle njSetLightRange njLight \
    njSetCamera njSetCameraAngle njPointCamereInterest njRollCameraInterest njPushMatrix njPopMatrix njTranslate \
    njTranslateV njScale njScaleV njGetTranslation njCalcPoint njInnerProduct njUnitVector njScalor njScalor2 \
    njSqrt njSin njCos njTan; do
    ninja_asm $f
done
# NAME and its Ninja sources, each by its inline body and its C in xbox/
ninja() {
    name=$1
    shift
    asm= c=
    for f in "$@"; do
        case $f in
        +*) c="$c $x/${f#+}.c" ;;
        *) asm="$asm $b/${f}_inline.s" c="$c $x/$f.c" ;;
        esac
    done
    run "$name" "$asm" "$c $m"
}
run math $o/src/system/math.s xbox/src/system/math.c
run vector $o/src/system/vector.s xbox/src/system/vector.c
run mesh $o/src/collision/mesh.s "xbox/src/collision/mesh.c $m"
run palette $o/src/draw/palette.s xbox/src/draw/palette.c
run adv0100_palette $o/modules/adv0100/palette.s xbox/modules/adv0100/palette.c
run xmtrx "$o/src/system/crt0.s $o/src/system/cache.s" "xbox/src/system/crt0.c xbox/src/system/cache.c"
ninja ninja1 njAddVector njSubVector njOuterProduct njFraction njRoundOff njGetMatrix
ninja ninja2 njUnitMatrix njSetMatrix njInitMatrix
ninja ninja3 njRotateX njRotateY njRotateZ
# the combined rotations call the single ones, whose C the x86 run links
ninja ninja4 njRotateXYZ njRotateZXY njRotate +njRotateX +njRotateY +njRotateZ
ninja ninja5 njSetPerspective njRotateViewXAbsolute njRotateViewXRelative njRotateViewYRelative \
    njRotateViewZAbsolute njRotateViewZRelative njTranslateViewAbsolute njTranslateViewRelative
ninja ninja6 njCalcVector njClipZ njProjectScreen
# these call the rotations, which they link as they are, and standins.h's
ninja ninja7 njMultiMatrix njMirror njRotateX njRotateY
ninja ninja8 njSetView njInitView njRotateX njRotateY njRotateZ
ninja ninja10 njInvertMatrix
ninja ninja11 njDetMatrix njClearMatrix njMultiMatrix
ninja ninja9 njSetLightAngle njSetLightRange njLight njMultiMatrix njCalcVector
# njCreateLight: make dc's split C, whose bodies run through pairs.py and
# emulate.py, on the SH-4
SH_FLOATS="--shc r10 -double=float" run ninja12 "$d/sdk/ninja/njCreateLight.c $b/njUnitMatrix_inline.s" \
    "$x/njCreateLight.c $x/njUnitMatrix.c $m"
# njLtSrcs.c's calculators: make dc's split C on the SH-4, make pc's, its
# bodies translated by tools/sh2c.py and its text through the float pass,
# on x86, with the port's njSqrt, which they call
SH_FLOATS="--shc r10 -double=float" run lights "$d/sdk/ninja/njLtSrcs.c $d/sdk/ninja/njLtMath.o" \
    "build/pc/sdk/ninja/njLtSrcs.i build/pc/sdk/ninja/njLtMath.i $x/njSqrt.c"
# the camera functions, whose bodies tools/sh2c.py translates, by make pc's
# text, with the rotations and the rest they call
p=build/pc/sdk/ninja
run camera "$b/njSetCamera_inline.s $b/njSetCameraAngle_inline.s $b/njPointCamereInterest_inline.s \
    $b/njRollCameraInterest_inline.s $b/njRotateX_inline.s $b/njRotateY_inline.s $b/njRotateZ_inline.s \
    $b/njRotate_inline.s $b/njMultiMatrix_inline.s $b/njCalcVector_inline.s $b/njUnitMatrix_inline.s" \
    "$p/njSetCamera.i $p/njSetCameraAngle.i $p/njPointCamereInterest.i $p/njRollCameraInterest.i \
    $x/njRotateX.c $x/njRotateY.c $x/njRotateZ.c $x/njRotate.c $x/njMultiMatrix.c $x/njCalcVector.c \
    $x/njUnitMatrix.c $m"
# Ninja's matrix stack, translations, scales and the rest: their shipped
# bodies, which ninja_asm made above, on the SH-4; the port's C on x86
asm= c=
for f in njPushMatrix njPopMatrix njTranslate njTranslateV njScale njScaleV njGetTranslation njCalcPoint \
    njInnerProduct njUnitVector njScalor njScalor2 njSqrt njSin njCos njTan; do
    asm="$asm $b/${f}_inline.s" c="$c $x/$f.c"
done
run ninja13 "$asm" "$c $m"
# the threads: make dc's split C of thread.c on the SH-4, make pc's on x86
SH_FLOATS="--shc r32" run thread $d/src/system/thread.c build/pc/src/system/thread.c
exit $status
