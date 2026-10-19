"""Inspect the linked Windows native gameplay DLL, including actual unwind data.

Does not claim Windows/Proton execution. The host bridge tests separately run
the real assembly through original and handled exits with changing FP state.
"""
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT/'build/RE7_OTS_Native.dll'


class PE:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.nt = self.u32(60)
        self.op = self.nt + 24
        self.imagebase = self.u64(self.op + 24)
        self.sections = []
        for i in range(self.u16(self.nt + 6)):
            p = self.op + self.u16(self.nt + 20) + 40*i
            self.sections.append((self.u32(p+12), self.u32(p+8), self.u32(p+16),
                                  self.u32(p+20), self.u32(p+36)))

    def u16(self, p): return struct.unpack_from('<H', self.data, p)[0]
    def u32(self, p): return struct.unpack_from('<I', self.data, p)[0]
    def u64(self, p): return struct.unpack_from('<Q', self.data, p)[0]
    def cstr(self, p): return self.data[p:self.data.index(0, p)].decode('ascii')

    def raw(self, rva):
        for va, _, size, offset, _ in self.sections:
            if va <= rva < va + size:
                return offset + rva - va
        raise AssertionError(f'Unmapped file RVA {rva:X}')

    def directory(self, index):
        p = self.op + 112 + index*8
        return self.u32(p), self.u32(p+4)

    def symbols(self):
        table, count = self.u32(self.nt+12), self.u32(self.nt+16)
        strings, result, i = table + 18*count, {}, 0
        while i < count:
            p = table + i*18
            if self.data[p:p+4] == b'\0'*4:
                name = self.cstr(strings+self.u32(p+4))
            else:
                name = self.data[p:p+8].split(b'\0')[0].decode('ascii')
            section = struct.unpack_from('<h', self.data, p+12)[0]
            if section > 0:
                result[name] = self.sections[section-1][0] + self.u32(p+8)
            i += 1 + self.data[p+17]
        return result

    def runtime_functions(self):
        if hasattr(self, '_runtime_functions'):
            return self._runtime_functions
        rva, size = self.directory(3)
        p = self.raw(rva)
        assert size % 12 == 0
        self._runtime_functions = [struct.unpack_from('<III', self.data, i) for i in range(p, p+size, 12)]
        return self._runtime_functions


def unwind_linked(pe, control_pc, context, memory):
    """Interpret actual version-1 UNWIND_INFO using documented Win64 rules.

    Handles the opcodes emitted by this bridge, including partial prologues,
    fixed nonvolatile XMM saves, a dynamic frame register, and terminal machine
    frames. A normal frame pops [RSP]; a machine frame explicitly replaces RIP
    and RSP. Unknown opcodes fail instead of silently passing a shape check.
    """
    functions = [entry for entry in pe.runtime_functions() if entry[0] <= control_pc < entry[1]]
    if len(functions) != 1:
        raise AssertionError('No unique described bridge frame at control PC')
    begin, _, unwind_rva = functions[0]
    q = pe.raw(unwind_rva)
    if pe.data[q] != 1:
        raise AssertionError('Unsupported unwind version/flags')
    prolog, count, frame_byte = pe.data[q+1:q+4]
    frame_register, frame_offset = frame_byte & 15, (frame_byte >> 4)*16
    relative = control_pc-begin
    codes, i = [], 0
    while i < count:
        code_offset, encoded = pe.data[q+4+2*i:q+6+2*i]
        opcode, info = encoded & 15, encoded >> 4
        operand = 0
        slots = 1
        if opcode in (4, 8) or (opcode == 1 and info == 0):
            operand = pe.u16(q+6+2*i)*(16 if opcode == 8 else 8)
            slots = 2
        elif opcode in (5, 9) or (opcode == 1 and info == 1):
            operand = pe.u32(q+6+2*i)
            slots = 3
        codes.append((code_offset, opcode, info, operand))
        i += slots
    if i != count:
        raise AssertionError('Invalid unwind code slot count')
    state = dict(context)
    state['xmm'] = dict(context['xmm'])
    fp_established = frame_register and (relative >= prolog or any(
        op == 3 and offset <= relative for offset, op, _, _ in codes))
    frame_base = state[frame_register]-frame_offset if fp_established else state[4]
    for offset, op, info, operand in codes:
        if relative < prolog and offset > relative:
            continue
        if op == 0:
            state[info] = memory[state[4]]
            state[4] += 8
        elif op == 1:
            state[4] += operand
        elif op == 2:
            state[4] += info*8+8
        elif op == 3:
            state[4] = frame_base
        elif op in (4, 5):
            state[info] = memory[frame_base+operand]
        elif op in (8, 9):
            address = frame_base+operand
            if address % 16:
                raise AssertionError('Nonvolatile XMM unwind save is unaligned')
            state['xmm'][info] = memory[address]
        elif op == 10:
            machine = state[4] + (8 if info else 0)
            state['rip'] = memory[machine]
            state[4] = memory[machine+24]
            return state
        else:
            raise AssertionError(f'Unsupported unwind opcode {op}')
    state['rip'] = memory[state[4]]
    state[4] += 8
    return state


@unittest.skipUnless(DLL.is_file(), 'Build RE7_OTS_Native.dll before binary checks')
class NativeBinaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pe = PE(DLL)

    def test_x64_abi_exports_imports_and_section_protection(self):
        p = self.pe
        self.assertEqual(p.data[:2], b'MZ')
        self.assertEqual(p.data[p.nt:p.nt+4], b'PE\0\0')
        self.assertEqual(p.u16(p.nt+4), 0x8664)
        self.assertTrue(p.u16(p.nt+22) & 0x2000)
        self.assertEqual(p.u16(p.op), 0x20B)
        self.assertNotEqual(p.u32(p.op+16), 0)
        self.assertEqual(p.u32(p.nt+8), 0, 'reproducible PE timestamp')
        self.assertEqual(p.u16(p.op+70) & 0x160, 0x160, 'ASLR/high entropy/NX')
        for directory in (3, 5):
            self.assertGreater(p.directory(directory)[0], 0)
            self.assertGreater(p.directory(directory)[1], 0)
        self.assertFalse(any(flags & 0x20000000 and flags & 0x80000000
                             for _, _, _, _, flags in p.sections), 'RWX section')
        q = p.raw(p.directory(1)[0])
        dependencies, imports = [], []
        while any(p.data[q:q+20]):
            dependencies.append(p.cstr(p.raw(p.u32(q+12))))
            thunk = p.raw(p.u32(q) or p.u32(q+16))
            while p.u64(thunk):
                entry = p.u64(thunk)
                self.assertFalse(entry >> 63, 'Unexpected ordinal import')
                imports.append(p.cstr(p.raw(entry)+2))
                thunk += 8
            q += 20
        self.assertEqual(set(dependencies), {'KERNEL32.dll', 'USER32.dll', 'msvcrt.dll'})
        self.assertFalse(set(imports) & {
            'VirtualProtectEx', 'CreateRemoteThread', 'LoadLibraryA', 'LoadLibraryW',
            'WinHttpOpen', 'WSAStartup', 'OpenProcess',
        })
        # This DLL deliberately writes the current process's selected game data.
        for required in ('ReadProcessMemory', 'WriteProcessMemory', 'GetCurrentProcess',
                         'VirtualProtect', 'FlushInstructionCache'):
            self.assertIn(required, imports)
        q = p.raw(p.directory(0)[0])
        self.assertEqual(p.u32(q+20), 2)
        self.assertEqual(p.u32(q+24), 2)
        names = p.raw(p.u32(q+32))
        self.assertEqual({p.cstr(p.raw(p.u32(names+4*i))) for i in range(2)},
                         {'reframework_plugin_required_version', 'reframework_plugin_initialize'})

    def test_all_58_completed_frames_execute_linked_unwind_to_original_hook_and_game_rsp(self):
        p = self.pe
        symbols = p.symbols()
        bridges = {name: rva for name, rva in symbols.items() if re.fullmatch(r'native_detour_\d+', name)}
        self.assertEqual(set(bridges), {f'native_detour_{n}' for n in range(58)})
        self.assertTrue('native_original_ips' in symbols, 'Original hook PC table is required')
        functions = p.runtime_functions()
        table = p.raw(symbols['native_detours'])
        saved = {3: 288, 5: 296, 6: 304, 7: 312, 12: 320, 13: 328, 14: 336, 15: 344}
        mutable = {3: 104, 5: 64, 6: 80, 7: 72, 12: 24, 13: 16, 14: 8, 15: 0}
        for n in range(58):
            with self.subTest(bridge=n):
                entry = symbols[f'native_detour_{n}']
                self.assertEqual(p.u64(table+8*n), p.imagebase+entry)
                original_pc = 0x140000000 + 0x10000*n
                for incoming_alignment in (0, 8):
                    game_rsp = 0x100000 + incoming_alignment
                    temporary = game_rsp-40
                    fixed = (temporary-392) & ~15
                    old_gprs = {register: 0x120000+register for register in saved}
                    old_xmms = {register: bytes([register])*16 for register in range(6, 16)}
                    memory = {game_rsp: 0xBADBADBADBADBAD, fixed+352: original_pc,
                              fixed+360: 0x33, fixed+368: 0x246,
                              fixed+376: game_rsp, fixed+384: 0x2b}
                    for register, offset in saved.items():
                        memory[fixed+offset] = old_gprs[register]
                        # Mutable output frame deliberately disagrees with the
                        # immutable unwind copy: callback writes must not corrupt it.
                        memory[fixed+mutable[register]] = 0xD00000+register
                    for register in range(6, 16):
                        memory[fixed+128+(register-6)*16] = old_xmms[register]
                    for region in ('body', 'prepare_original', 'restore_handled', 'restore_original'):
                        begin = symbols[f'native_{region}_{n}']
                        end, = [end for start, end, _ in functions if start == begin]
                        # Every restore instruction boundary includes the point
                        # after R12 has been overwritten. Body RSP can be deep in
                        # page probing, below a flags push, or at callback return.
                        sites = [begin, end-1]
                        if region.startswith('restore'):
                            raw = p.raw(begin)
                            cursor = 0
                            while begin+cursor < end:
                                sites.append(begin+cursor)
                                prefix, opcode, modrm, sib = p.data[raw+cursor:raw+cursor+4]
                                self.assertIn(prefix, (0x48, 0x4c))
                                self.assertEqual((opcode, modrm & 7, sib), (0x8b, 4, 0x24))
                                cursor += 4 + (1 if modrm >> 6 == 1 else 4 if modrm >> 6 == 2 else 0)
                            self.assertEqual(begin+cursor, end)
                        for site in sites:
                            for depth in (0, 8, 512, 8192, 65536):
                                rsp = fixed if region.startswith('restore') else fixed-depth
                                context = {register: 0xAB0000+register for register in range(16)}
                                context.update({4: rsp, 12: fixed,
                                    'xmm': {register: bytes([0xee])*16 for register in range(6, 16)}})
                                if region.startswith('restore'):
                                    context[12] = 0xD0000C  # restored R12 is not the frame anchor
                                result = unwind_linked(p, site, context, memory)
                                self.assertEqual(result['rip'], original_pc)
                                self.assertEqual(result[4], game_rsp)
                                self.assertNotEqual(result['rip'], memory[game_rsp])
                                for register in saved:
                                    self.assertEqual(result[register], old_gprs[register])
                                self.assertEqual(result['xmm'], old_xmms)

    def test_partial_setup_unwind_and_explicit_landing_transfer_boundaries(self):
        p, symbols = self.pe, self.pe.symbols()
        functions = p.runtime_functions()
        for n in range(58):
            entry, setup = symbols[f'native_detour_{n}'], symbols[f'native_setup_{n}']
            code = p.data[p.raw(entry):p.raw(entry)+6]
            self.assertEqual(code[:2], bytes.fromhex('FF 35'))
            self.assertEqual(entry+6+struct.unpack_from('<i', code, 2)[0],
                             symbols['native_original_ips']+8*n)
            self.assertEqual(setup, entry+6)
            # Static metadata intentionally makes no guarantee before the first
            # PUSH exists or after original S is restored for the terminal JMP.
            self.assertFalse(any(begin <= entry < end for begin, end, _ in functions))
            for path in ('handled', 'original'):
                transfer = symbols[f'native_transfer_{path}_{n}']
                self.assertFalse(any(begin <= transfer < end for begin, end, _ in functions))
                self.assertEqual(p.data[p.raw(transfer):p.raw(transfer)+2], bytes.fromhex('FF 25'))
            game_rsp, original_pc = 0x200008, 0x140020000+n*16
            temporary, original_r12 = game_rsp-40, 0x121212121212
            memory = {game_rsp: 0xBADBADBAD, game_rsp-8: original_pc,
                      game_rsp-16: 0x246, game_rsp-24: 0xAAAA,
                      game_rsp-32: 0xDDDD, temporary: original_r12}
            # Offsets are checked against linked machine code, not guessed
            # instruction lengths; setup saves are annotated as they execute.
            self.assertEqual(p.data[p.raw(setup):p.raw(setup)+8], bytes.fromhex('9C 50 52 41 54 49 89 E4'))
            for offset, rsp, frame in ((0, game_rsp-8, original_r12),
                                       (1, game_rsp-16, original_r12),
                                       (2, game_rsp-24, original_r12),
                                       (3, game_rsp-32, original_r12),
                                       (5, temporary, original_r12),
                                       (8, temporary, temporary),
                                       (23, temporary-400, temporary)):
                xmm = {register: bytes([register])*16 for register in range(6, 16)}
                context = {register: 0x10000+register for register in range(16)}
                context.update({4: rsp, 12: frame, 'xmm': xmm})
                result = unwind_linked(p, setup+offset, context, memory)
                self.assertEqual((result['rip'], result[4], result[12]),
                                 (original_pc, game_rsp, original_r12))
                self.assertEqual(result['xmm'], xmm)
                for register in (3, 5, 6, 7, 13, 14, 15):
                    self.assertEqual(result[register], context[register])

    def test_linked_bridges_use_full_xsave_mask_and_fallback_around_native_dispatch(self):
        p = self.pe
        symbols = p.symbols()
        # Validate linked instruction operands, not source spelling. Each XSAVE
        # and XRSTOR consumes EDX:EAX loaded from both halves of the same mask.
        for n in range(58):
            with self.subTest(bridge=n):
                begin = symbols[f'native_detour_{n}']
                code = p.data[p.raw(begin):p.raw(begin)+(symbols[f'native_end_{n}']-begin)]
                positions = []
                for instruction in ('49 0F AE 65 00', '49 0F AE 6D 00'):
                    opcode = bytes.fromhex(instruction)
                    self.assertEqual(code.count(opcode), 1)
                    i = code.index(opcode)
                    self.assertEqual(code[i-12:i-10], bytes.fromhex('8B 05'))
                    self.assertEqual(code[i-6:i-4], bytes.fromhex('8B 15'))
                    low = begin+i-6+struct.unpack_from('<i', code, i-10)[0]
                    high = begin+i+struct.unpack_from('<i', code, i-4)[0]
                    self.assertEqual(low, symbols['native_xsave_mask'])
                    self.assertEqual(high, symbols['native_xsave_mask']+4)
                    positions.append(i)
                for instruction in ('49 0F AE 45 00', '49 0F AE 4D 00'):
                    self.assertEqual(code.count(bytes.fromhex(instruction)), 1)
                setup = b'\xB9'+struct.pack('<I', n)+bytes.fromhex('4C 89 E2 4D 89 E8 E8')
                self.assertEqual(code.count(setup), 1)
                call = code.index(setup)+len(setup)-1
                target = begin+call+5+struct.unpack_from('<i', code, call+1)[0]
                self.assertEqual(target, symbols['native_dispatch'])
                self.assertLess(positions[0], call)
                self.assertLess(call, positions[1])
                # Fixed indirect exits must target this detour's own slots.
                jumps = []
                for match in re.finditer(b'\xFF\x25', code):
                    i = match.start()
                    jumps.append(begin+i+6+struct.unpack_from('<i', code, i+2)[0])
                self.assertIn(symbols['native_returns']+n*8, jumps)
                self.assertIn(symbols['native_trampolines']+n*8, jumps)
