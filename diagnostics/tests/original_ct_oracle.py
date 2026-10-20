"""Private pinned CT -> host x87 arithmetic oracle. Never republishes the CT.

Extracts original instructions, not a second Python/C version of the equations.
General registers/object loads are replaced by resolved scalar inputs. No branch
or game memory is executed. This tests arithmetic only, not game hook semantics.
"""
import ctypes
import hashlib
import re
import subprocess

CT_SHA256 = '4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'
RANGES = {
    'ots_x': (5621, 5640, 'camxthird'),
    'ots_y': (5739, 5747, 'camythird'),
    'ots_z': (6303, 6322, 'camzthird'),
    'fp_x': (5671, 5684, 'camxthird'),
    'fp_z': (6335, 6348, 'camzthird'),
    'pitchmul': (5807, 5813, 'pitchmul'),
}

class Oracle:
    def __init__(self, path, output):
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != CT_SHA256:
            raise ValueError('Original CT hash does not match RT 8.02 reference')
        lines = raw.decode('utf-8-sig').splitlines()
        self.fields, self.results = {}, {}
        assembly = ['.text']
        for name, (start, end, result) in RANGES.items():
            assembly += [f'.globl oracle_{name}', f'oracle_{name}:']
            fields = []
            for line in lines[start - 1:end]:
                op = line.split('//')[0].strip()
                if op == 'fabs':
                    assembly.append('fabs')
                    continue
                m = re.fullmatch(r'(fld|fmul|fsub|fadd|fstp) \[(\w+)\]', op)
                if not m:
                    if op.startswith(('f', 'j')):
                        raise ValueError(f'Unexpected instruction in numeric range: {op}')
                    continue  # general-register input moves, already resolved
                opcode, field = m.groups()
                if field not in fields:
                    fields.append(field)
                mnemonic = {'fld': 'flds', 'fmul': 'fmuls', 'fsub': 'fsubs',
                            'fadd': 'fadds', 'fstp': 'fstps'}[opcode]
                assembly.append(f'{mnemonic} {4 * fields.index(field)}(%rdi)')
            assembly.append('ret')
            self.fields[name], self.results[name] = fields, result
        assembly.append('.section .note.GNU-stack,"",@progbits')
        src, so = output/'oracle.S', output/'oracle.so'
        src.write_text('\n'.join(assembly) + '\n')
        subprocess.run(['cc', '-shared', '-fPIC', str(src), '-o', str(so)], check=True)
        self.lib = ctypes.CDLL(str(so))
        for name in RANGES:
            fn = getattr(self.lib, 'oracle_' + name)
            fn.argtypes, fn.restype = [ctypes.POINTER(ctypes.c_float)], None

    def run(self, name, values):
        fields = self.fields[name]
        cells = (ctypes.c_float * len(fields))(*(values.get(f, 0) for f in fields))
        getattr(self.lib, 'oracle_' + name)(cells)
        return cells[fields.index(self.results[name])]
