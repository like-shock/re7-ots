import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
import warnings
import zipfile

TOOL = Path(__file__).resolve().parents[1] / 'tools/inspect_model_package.py'


class ModelPackageTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(TOOL.exists(), 'model package inspector not implemented')
        spec = importlib.util.spec_from_file_location('model_package', TOOL)
        self.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.mod)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def archive(self, entries):
        path = self.root / 'model.zip'
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
                for name, data in entries:
                    z.writestr(name, data)
        return path

    def test_inventory_matches_directory_and_does_not_infer_compatibility(self):
        entries = [('Model/natives/stm/test.mesh.12345', b'fixture mesh'),
                   ('Model/natives/stm/test.tex.67890', b'fixture texture'),
                   ('Model/modinfo.ini', b'name=Ethan RT\n')]
        path = self.archive(entries)
        extracted = self.root / 'extracted'
        for name, data in entries:
            p = extracted / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        result = self.mod.inspect_package(path)
        self.assertEqual(result['files'], self.mod.inspect_package(extracted)['files'])
        self.assertEqual(result['source']['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(result['file_count'], 3)
        self.assertEqual(result['total_bytes'], sum(len(data) for _, data in entries))
        mesh = next(f for f in result['files'] if f['kind'] == 'mesh')
        self.assertEqual(mesh['format_suffix'], '12345')
        self.assertEqual(mesh['sha256'], hashlib.sha256(b'fixture mesh').hexdigest())
        self.assertEqual(result['compatibility'], 'UNVERIFIED')
        self.assertNotIn(str(self.root), json.dumps(result))
        self.assertEqual(result, self.mod.inspect_package(path))
        self.assertEqual(sorted(p.relative_to(extracted).as_posix() for p in extracted.rglob('*') if p.is_file()),
                         sorted(name for name, _ in entries))

    def test_unsafe_or_ambiguous_zip_paths_are_rejected(self):
        for entries in [[('../escape', b'a')], [('/absolute', b'a')], [('C:\\x', b'a')],
                        [('a/./b', b'a')], [('a', b'a'), ('a', b'b')],
                        [('a', b'a'), ('a/b', b'b')], [('a/b', b'b'), ('a', b'a')],
                        [('A/test', b'a'), ('a/TEST', b'b')],
                        [('a\\test', b'a'), ('a/test', b'b')]]:
            with self.subTest(entries=entries):
                with self.assertRaises(ValueError):
                    self.mod.inspect_package(self.archive(entries))

    def test_symlinks_and_special_entries_are_rejected(self):
        info = zipfile.ZipInfo('link')
        info.create_system = 3
        info.external_attr = 0o120777 << 16
        with self.assertRaises(ValueError):
            self.mod.inspect_package(self.archive([(info, b'outside')]))
        folder = self.root / 'folder'
        folder.mkdir()
        (folder / 'link').symlink_to(self.root)
        with self.assertRaises(ValueError):
            self.mod.inspect_package(folder)

    def test_limits_and_invalid_archives_report_errors(self):
        path = self.archive([('test.mesh.1', b'x' * 100)])
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path, max_bytes=99)
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path, max_files=0)
        empty = self.archive([])
        with self.assertRaises(ValueError):
            self.mod.inspect_package(empty)
        for name, data in [('bad.zip', b'not a zip'), ('model.7z', b'7z'), ('model.rar', b'Rar!')]:
            bad = self.root / name
            bad.write_bytes(data)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.mod.inspect_package(bad)

    def test_crc_corruption_is_not_accepted(self):
        path = self.root / 'broken.zip'
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_STORED) as z:
            z.writestr('test.mesh.1', b'UNIQUE_PAYLOAD')
        path.write_bytes(path.read_bytes().replace(b'UNIQUE_PAYLOAD', b'BROKEN_PAYLOAD'))
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path)

    def test_path_depth_length_and_implicit_directories_are_bounded(self):
        path = self.archive([('a/b/c/d/test.mesh.1', b'x')])
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path, max_files=4)
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path, max_depth=4)
        self.assertEqual(self.mod.inspect_package(path, max_depth=5, max_files=5)['file_count'], 1)
        path = self.archive([('a' * 4097, b'x')])
        with self.assertRaises(ValueError):
            self.mod.inspect_package(path)

    def test_implicit_parent_case_conflicts_are_rejected(self):
        for entries in [[('A/one',b'a'),('a/two',b'b')],
                        [('root/A/one',b'a'),('root/a/two',b'b')],
                        [('A/one',b'a'),('a/',b'')]]:
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                self.mod.inspect_package(self.archive(entries))
        self.assertEqual(self.mod.inspect_package(self.archive([('A/one',b'a'),('A/two',b'b')]))['file_count'],2)

    def test_corrupt_lzma_cli_returns_clean_error(self):
        path=self.root/'lzma.zip'
        with zipfile.ZipFile(path,'w',zipfile.ZIP_LZMA) as z:
            z.writestr('model.mesh.1',b'payload'*100)
        raw=bytearray(path.read_bytes())
        start=30+struct.unpack_from('<H',raw,26)[0]+struct.unpack_from('<H',raw,28)[0]
        raw[start+4]=255  # Invalid LZMA properties byte, before CRC validation.
        path.write_bytes(raw)
        result=subprocess.run([sys.executable,str(TOOL),str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2,result.stderr)
        self.assertNotIn('Traceback',result.stderr)
        self.assertEqual(result.stdout,'')

    def test_cli_json_and_errors(self):
        path = self.archive([('test.mesh.1', b'mesh')])
        result = subprocess.run([sys.executable, str(TOOL), str(path)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['file_count'], 1)
        bad = subprocess.run([sys.executable, str(TOOL), str(self.root / 'missing.zip')], capture_output=True, text=True)
        self.assertEqual(bad.returncode, 2)
        self.assertEqual(bad.stdout, '')
        self.assertNotIn('Traceback', bad.stderr)


if __name__ == '__main__':
    unittest.main()
