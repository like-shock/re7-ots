"""Real option state/persistence bytes and embedded UI callback contracts."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class NativeOptionsTests(unittest.TestCase):
    def test_persisted_values_pending_changes_and_save_failures(self):
        self.compile_run('native_options_host',False)
    def test_embedded_checkbox_registration_click_and_lua_reset(self):
        self.compile_run('native_options_ui_host',True)
    def test_embedded_checkbox_with_pinned_lua_core(self):
        self.compile_run('native_options_ui_host',True,True)
    def compile_run(self,name,ui,pinned=False):
        names=['native_options']+(['native_options_ui'] if ui else [])
        for source in names:
            self.assertTrue((ROOT/'src'/f'{source}.c').is_file(),'native options implementation missing')
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/name
            cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),'-I'+str(ROOT/'build/lua-5.4.3/src'),str(ROOT/'tests'/f'{name}.c')]
            cmd += [str(ROOT/'src'/f'{source}.c') for source in names]
            if pinned:
                cmd += [str(p) for p in (ROOT/'build/lua-5.4.3/src').glob('*.c') if p.stem not in ('lua','luac')] + ['-lm']
            elif ui:cmd+=['-Wl,-l:liblua5.4.so.0']
            subprocess.run(cmd+['-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
