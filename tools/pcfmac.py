"""Give the Xbox port's x86 object the FMACs SHC fuses: rewrite in place each
single-precision multiply-add that clang contracted in LLVM IR as a call of
the port's FMAC, port_fmac (xbox/port/fpu.c).

    python3 tools/pcfmac.py IR

IR is LLVM's text of a source's .i, as pc.mk's pc macro writes it for an
object of R26's or R32's route: the float pass (tools/floats.py) has left
for contraction just the products SHC fuses, and clang, under
-ffp-contract=on, contracts each into its run's sum, as make dc's GCC does,
by llvm.fmuladd. The Pentium III has no fused multiply-add, so the backend
would split each into a product and a sum, each rounded. port_fmac rounds
once, as the console's FMAC does. Under -frounding-math, clang writes the
constrained form, llvm.experimental.constrained.fmuladd.f32, and the tool
takes that and the plain llvm.fmuladd.f32 alike, and declares port_fmac in
place of the intrinsic.

The console fuses floats alone: its FMAC is single precision, and
-m4-single-only makes make dc's every double a float. The tool fails, and
leaves IR as it was, on a multiply-add of another type, which would want
a rule of its own.
"""

import re
import sys

# a call of a float multiply-add: its three operands, which hold no comma
# of their own (an SSA value, a constant or a float literal), and the
# constrained form's rounding and exception metadata
CALL = re.compile(
    r'@llvm\.(?:experimental\.constrained\.)?fmuladd\.f32\('
    r'(float [^,()]+), (float [^,()]+), (float [^,()]+)'
    r'(?:, metadata !"round\.dynamic", metadata !"fpexcept\.(?:ignore|strict|maytrap)")?\)'
)
DECLARE = re.compile(r'^declare (?:[\w ]+ )?float @llvm\.(?:experimental\.constrained\.)?fmuladd\.f32\(.*$', re.M)
OTHER = re.compile(r'@llvm\.(?:experimental\.constrained\.)?fmuladd\.(?!f32\()[\w.]+\(')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    text = open(path).read()
    other = OTHER.search(text)
    if other:
        sys.exit(f'{path}: a multiply-add of another type than float, {other.group(0)}: give it a rule first')
    text, calls = CALL.subn(r'@port_fmac(\1, \2, \3)', text)
    for line in text.splitlines():
        if 'fmuladd.f32(' in line and not line.startswith('declare'):
            sys.exit(f'{path}: a float multiply-add the tool can\'t read: {line.strip()}')
    if calls:
        text, declared = DECLARE.subn('declare float @port_fmac(float, float, float)', text)
        if declared > 1:
            sys.exit(f'{path}: {declared} declarations of the multiply-add')
        if declared == 0 and 'declare float @port_fmac(' not in text:
            text += '\ndeclare float @port_fmac(float, float, float)\n'
    open(path, 'w').write(text)


if __name__ == '__main__':
    main()
