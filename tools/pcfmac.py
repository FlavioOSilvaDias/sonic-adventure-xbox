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
takes that and the plain llvm.fmuladd.f32 alike, and drops the intrinsic's
declaration.

The console fuses floats alone: its FMAC is single precision, and
-m4-single-only makes make dc's every double a float. The tool fails, and
leaves IR as it was, on a multiply-add of another type, which would want
a rule of its own.

The call is to @port_fmac.inline, which the tool defines in the module,
internal and always inlined: port_fmac's body (xbox/port/fpu.c) in IR, so
that a fused product costs no call: the console's FMAC, the operands'
denormals as their signed zeros, the product and sum in double (the x87,
which the hosts run at 53 bits rounding toward zero), rounded once to a
float, and a result below the normal range a signed zero. Its
operations are the constrained ones -frounding-math gives the callers,
strictfp as they are: LLVM inlines no strictfp function into a caller that
isn't, so the build's -frounding-math (config/pc.mk's PC_AS_SHC) is what
lets it inline. xbox/test/fpu_exact.sh compiles a product through
the tool and checks it against port_fmac.
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
DECLARE = re.compile(r'^declare (?:[\w ]+ )?float @llvm\.(?:experimental\.constrained\.)?fmuladd\.f32\(.*\n?', re.M)
OTHER = re.compile(r'@llvm\.(?:experimental\.constrained\.)?fmuladd\.(?!f32\()[\w.]+\(')


# the constrained operations' rounding and exceptions, as -frounding-math
# gives them
ROUND = 'metadata !"round.dynamic", metadata !"fpexcept.ignore"'
EXCEPT = 'metadata !"fpexcept.ignore"'
CONSTRAINED = 'llvm.experimental.constrained.'
# the intrinsics the body calls, and their declarations
INTRINSICS = {
    'fpext.f64.f32': 'double (float, metadata)',
    'fmul.f64': 'double (double, double, metadata, metadata)',
    'fadd.f64': 'double (double, double, metadata, metadata)',
    'fptrunc.f32.f64': 'float (double, metadata, metadata)',
    'fcmp.f32': 'i1 (float, float, metadata, metadata)',
}
# and the plain ones, which no rounding changes
PLAIN = {
    'llvm.fabs.f32': 'float (float)',
    'llvm.copysign.f32': 'float (float, float)',
}


def flushed(name):
    """IR that makes %NAME.f float %NAME with a denormal as its signed
    zero, in SSE's float operations, as the Pentium III's SSE has no move
    from its registers to the integer ones: a magnitude below the normal
    range's least, 2^-126, which a zero's is too, takes the sign alone. MXCSR's
    denormals as zero, where the processor has it, leaves the comparison
    true for a denormal"""
    return (f'  %{name}.abs = call float @llvm.fabs.f32(float %{name})\n'
            f'  %{name}.z = call i1 @{CONSTRAINED}fcmp.f32(float %{name}.abs, float 0x3810000000000000, '
            f'metadata !"olt", {EXCEPT}) strictfp\n'
            f'  %{name}.s = call float @llvm.copysign.f32(float 0.000000e+00, float %{name})\n'
            f'  %{name}.f = select i1 %{name}.z, float %{name}.s, float %{name}\n')


INLINE = (
    'define internal float @port_fmac.inline(float %a, float %b, float %c) alwaysinline nounwind strictfp '
    'null_pointer_is_valid {\n'
    'entry:\n'
    + flushed('a') + flushed('b') + flushed('c') +
    f'  %a.d = call double @{CONSTRAINED}fpext.f64.f32(float %a.f, {EXCEPT}) strictfp\n'
    f'  %b.d = call double @{CONSTRAINED}fpext.f64.f32(float %b.f, {EXCEPT}) strictfp\n'
    f'  %c.d = call double @{CONSTRAINED}fpext.f64.f32(float %c.f, {EXCEPT}) strictfp\n'
    f'  %pd = call double @{CONSTRAINED}fmul.f64(double %a.d, double %b.d, {ROUND}) strictfp\n'
    f'  %sd = call double @{CONSTRAINED}fadd.f64(double %pd, double %c.d, {ROUND}) strictfp\n'
    f'  %r = call float @{CONSTRAINED}fptrunc.f32.f64(double %sd, {ROUND}) strictfp\n'
    + flushed('r') +
    '  ret float %r.f\n'
    '}\n'
)


def define_inline(text):
    """TEXT with @port_fmac.inline defined, and what it calls declared
    where TEXT doesn't declare them"""
    out = ['', INLINE]
    for name, signature in INTRINSICS.items():
        if not re.search(rf'^declare [^\n]*@{re.escape(CONSTRAINED + name)}\(', text, re.M):
            result, arguments = signature.split(' ', 1)
            out.append(f'declare {result} @{CONSTRAINED}{name}{arguments}')
    for name, signature in PLAIN.items():
        if not re.search(rf'^declare [^\n]*@{re.escape(name)}\(', text, re.M):
            result, arguments = signature.split(' ', 1)
            out.append(f'declare {result} @{name}{arguments}')
    return text.rstrip('\n') + '\n' + '\n'.join(out) + '\n'


def rewrite(text, path='IR'):
    """TEXT with each float multiply-add a call of @port_fmac.inline"""
    other = OTHER.search(text)
    if other:
        sys.exit(f'{path}: a multiply-add of another type than float, {other.group(0)}: give it a rule first')
    text, calls = CALL.subn(r'@port_fmac.inline(\1, \2, \3)', text)
    for line in text.splitlines():
        if 'fmuladd.f32(' in line and not line.startswith('declare'):
            sys.exit(f'{path}: a float multiply-add the tool can\'t read: {line.strip()}')
    if calls:
        text, declared = DECLARE.subn('', text)
        if declared > 1:
            sys.exit(f'{path}: {declared} declarations of the multiply-add')
        if '@port_fmac.inline(float %a' in text:
            sys.exit(f'{path}: already rewritten')
        text = define_inline(text)
    return text


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    text = rewrite(open(path).read(), path)
    open(path, 'w').write(text)


if __name__ == '__main__':
    main()
