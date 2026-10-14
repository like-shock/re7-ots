"""Exercise actual packaging CLI in an isolated Git repository, without game assets."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile

TOOL = Path(__file__).resolve().parents[1]/'tools/package_explore.py'


class PackageExploreTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/'repo';self.root.mkdir()
        self.diag=self.root/'diagnostics'
        (self.diag/'tools').mkdir(parents=True)
        shutil.copyfile(TOOL,self.diag/'tools/package_explore.py')
        shutil.copyfile(TOOL.with_name('build_explore_lua.py'),self.diag/'tools/build_explore_lua.py')
        (self.diag/'lua/explore').mkdir(parents=True)
        (self.diag/'lua/model').mkdir()
        for name in ('explore/core','explore/journal','explore/trace','model/visibility','model/ethan','model/session'):
            (self.diag/f'lua/{name}.lua').write_text('return {}\n')
        (self.diag/'lua/explore/adapter.lua').write_text("local VERSION='0.4.1-dev'\n")
        for name in ('EXPLORE_README_KO.md','LICENSE','NOTICE.md','ORIGINAL_CREDITS.txt'):
            (self.diag/name).write_text(name+'\n')
        (self.root/'.gitignore').write_text('**/build/\ndist/\n')
        self.git('init','-q')
        self.git('config','user.email','test@example.invalid')
        self.git('config','user.name','Package test')
        self.git('add','.')
        self.git('commit','-qm','Fixture')
        (self.diag/'build').mkdir()
        (self.diag/'build/RE7_OTS_Explore.dll').write_bytes(b'test-only DLL payload')
        subprocess.run([sys.executable,str(self.diag/'tools/build_explore_lua.py')],check=True)

    def git(self,*args):
        return subprocess.check_output(['git','-C',str(self.root),*args],text=True).strip()

    def package(self,out='dist'):
        return subprocess.run([sys.executable,str(self.diag/'tools/package_explore.py'),str(self.root/out)],
                              capture_output=True,text=True)

    def test_repeatable_bytes_revision_and_complete_manifests(self):
        first=self.package();self.assertEqual(first.returncode,0,first.stderr)
        originals={p.name:p.read_bytes() for p in (self.root/'dist').glob('*.zip')}
        second=self.package('dist/again');self.assertEqual(second.returncode,0,second.stderr)
        self.assertEqual(len(originals),2)
        for path in (self.root/'dist/again').glob('*.zip'):
            self.assertEqual(path.read_bytes(),originals[path.name])
            with ZipFile(path) as z:
                self.assertIsNone(z.testzip())
                manifest=next(n for n in z.namelist() if n.endswith('SHA256SUMS'))
                prefix=manifest.removesuffix('SHA256SUMS')
                self.assertIn(prefix+'BUILD_INFO.json',z.namelist())
                info=json.loads(z.read(prefix+'BUILD_INFO.json'))
                self.assertEqual(info['commit'],self.git('rev-parse','HEAD'))
                self.assertEqual(info['tree'],self.git('rev-parse','HEAD^{tree}'))
                self.assertEqual(info['version'],'0.4.1-dev')
                entries=set()
                for line in z.read(manifest).decode().splitlines():
                    digest,name=line.split('  ',1);entries.add(prefix+name)
                    self.assertEqual(hashlib.sha256(z.read(prefix+name)).hexdigest(),digest)
                self.assertEqual(entries|{manifest},set(z.namelist()))
                self.assertTrue(all(i.date_time==(1980,1,1,0,0,0) for i in z.infolist()))

    def test_dirty_tracked_source_is_rejected_without_publishing(self):
        for staged in (False,True):
            with self.subTest(staged=staged):
                (self.diag/'NOTICE.md').write_text('uncommitted change')
                if staged:self.git('add','diagnostics/NOTICE.md')
                result=self.package()
                self.assertEqual(result.returncode,2,result.stdout+result.stderr)
                self.assertIn('uncommitted',result.stderr)
                self.assertNotIn('Traceback',result.stderr)
                self.assertFalse(list((self.root/'dist').glob('*.zip')))

    def test_stale_lua_version_is_rejected_before_writing(self):
        (self.diag/'build/re7_ots_explore.lua').write_text("local VERSION='0.3.0'\n")
        result=self.package()
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        self.assertIn('version',result.stderr)
        self.assertFalse(list((self.root/'dist').glob('*.zip')))

    def test_missing_build_has_clean_error(self):
        (self.diag/'build/RE7_OTS_Explore.dll').unlink()
        result=self.package()
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        self.assertNotIn('Traceback',result.stderr)
        self.assertFalse(list((self.root/'dist').glob('*.zip')))

    def test_stale_lua_with_same_version_is_rejected(self):
        path=self.diag/'build/re7_ots_explore.lua'
        path.write_text(path.read_text()+'\n-- stale generated content\n')
        result=self.package()
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        self.assertIn('Lua',result.stderr)
        self.assertFalse(list((self.root/'dist').glob('*.zip')))
