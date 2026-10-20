"""Publication must reject stale or damaged build artifacts before any upload."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SHA = "a" * 40
VERSION = "0.1.14"


class NativeReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.project = Path(self.tmp.name) / "repo"
        self.artifacts = Path(self.tmp.name) / "artifacts"
        self.artifacts.mkdir()
        self.project.mkdir()
        files = {
            "diagnostics/src/native_plugin.c": '#define NATIVE_VERSION "0.1.14"\n',
            "diagnostics/native_build.sh": "'version':'0.1.14'\n",
            "diagnostics/tools/package_native.py": "VERSION = '0.1.14'\n",
            "docs/release-notes/0.1.14.md": "## English\n\nInstallation guide.\n\n## 한국어\n\n설치 안내.\n",
        }
        for name, body in files.items():
            path = self.project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(body)
        self.payload = {name: b"fixture" for name in (
            "install.sh", "uninstall.sh", "README_KO.md", "README_EN.md",
            "MINHOOK_LICENSE.txt", "NOTICE.md", "ORIGINAL_CREDITS.txt", "LICENSE",
            "reframework/plugins/RE7_OTS_Native.dll")}
        self.info = {"schema": 1, "version": VERSION, "mode": "ORIGINAL_NATIVE_OTS_EXPERIMENTAL",
                     "commit": SHA, "source_commit": SHA, "dirty": False,
                     "dll_sha256": hashlib.sha256(self.payload["reframework/plugins/RE7_OTS_Native.dll"]).hexdigest()}
        spec = importlib.util.spec_from_file_location("prepare_native_release", ROOT / "diagnostics/tools/prepare_native_release.py")
        self.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.mod)

    def archive(self, change=None, info=None):
        payload = dict(self.payload)
        payload["BUILD_INFO.json"] = json.dumps(self.info | (info or {})).encode()
        payload["SHA256SUMS"] = "".join(f"{hashlib.sha256(v).hexdigest()}  {k}\n"
                                      for k, v in sorted(payload.items())).encode()
        if change:
            change(payload)
        self.zip = self.artifacts / f"RE7_OTS_Native_v{VERSION}_SteamDeck.zip"
        with zipfile.ZipFile(self.zip, "w") as z:
            for name, value in payload.items():
                z.writestr(name, value)

    def prepare(self, tag="v0.1.14", sha=SHA):
        return self.mod.prepare(self.project, self.artifacts, tag, sha)

    def test_verified_package_produces_upload_paths_and_matching_checksum(self):
        self.archive()
        result = self.prepare()
        self.assertEqual(result["version"], "0.1.14")
        self.assertEqual(Path(result["zip"]).read_bytes(), self.zip.read_bytes())
        self.assertEqual(Path(result["checksums"]).read_text(),
                         f"{hashlib.sha256(self.zip.read_bytes()).hexdigest()}  {self.zip.name}\n")
        self.assertEqual(Path(result["notes"]), self.project / "docs/release-notes/0.1.14.md")

    def test_tag_and_compiled_version_must_match(self):
        self.archive()
        for tag in ("main", "v0.1.13", "../v0.1.14", "v0.1.14-rc1"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                self.prepare(tag)
        (self.project / "diagnostics/src/native_plugin.c").write_text('#define NATIVE_VERSION "0.1.13"\n')
        with self.assertRaises(ValueError):
            self.prepare()

    def test_wrong_commit_dirty_build_version_and_dll_digest_are_rejected(self):
        for mutation in ({"commit": "b"*40}, {"source_commit": "b"*40}, {"dirty": True},
                         {"version": "0.1.13"}, {"dll_sha256": "0"*64},
                         {"schema": 2}, {"mode": "PASS_THROUGH_HOOK_PROBE"}):
            self.archive(info=mutation)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.prepare()

    def test_corruption_missing_guide_and_extra_files_are_rejected(self):
        changes = (
            lambda p: p.update({"install.sh": b"damaged"}),
            lambda p: p.pop("README_EN.md"),
            lambda p: p.update({"../unexpected": b"not an installer entry"}),
            lambda p: p.update({"SHA256SUMS": b""}),
        )
        for change in changes:
            self.archive(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.prepare()
        self.assertFalse((self.artifacts / "native-release").exists())

    def test_duplicate_zip_entry_is_rejected(self):
        self.archive()
        import warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            with zipfile.ZipFile(self.zip, "a") as z:
                z.writestr("README_EN.md", b"duplicate")
        with self.assertRaises(ValueError):
            self.prepare()

    def test_bilingual_notes_are_required_before_upload(self):
        self.archive()
        (self.project / "docs/release-notes/0.1.14.md").write_text("## English\nOnly English")
        with self.assertRaises(ValueError):
            self.prepare()
