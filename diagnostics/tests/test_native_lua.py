import pathlib, subprocess, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class NativeLuaTests(unittest.TestCase):
    def test_real_lua_registration_multiple_states_and_provider(self):
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Isrc',
                        '-Ibuild/lua-5.4.3/src', 'tests/native_lua_host.c', 'src/native_lua.c',
                        '-Wl,-l:liblua5.4.so.0', '-o', 'build/native_lua_host'], cwd=ROOT, check=True)
        subprocess.run([str(ROOT/'build/native_lua_host')], check=True)

    def test_bridge_linked_to_the_actual_pinned_core(self):
        core = ROOT/'build/lua-5.4.3/src'
        sources = [str(p) for p in core.glob('*.c') if p.stem not in ('lua','luac')]
        subprocess.run(['cc', '-std=c11', '-O2', '-Isrc', '-I'+str(core),
                        'tests/native_lua_host.c', 'src/native_lua.c', *sources,
                        '-lm', '-o', 'build/native_lua_pinned_host'], cwd=ROOT, check=True)
        subprocess.run([str(ROOT/'build/native_lua_pinned_host')], check=True)
