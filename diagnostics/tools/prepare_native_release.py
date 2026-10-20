"""Validate the tested Native artifact before giving a release job upload paths."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ENTRIES = {
    "reframework/plugins/RE7_OTS_Native.dll", "BUILD_INFO.json", "SHA256SUMS",
    "install.sh", "uninstall.sh", "README_KO.md", "README_EN.md",
    "MINHOOK_LICENSE.txt", "NOTICE.md", "ORIGINAL_CREDITS.txt", "LICENSE",
}


def prepare(project, artifacts, tag, commit):
    if not re.fullmatch(r"v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", tag):
        raise ValueError("A stable vMAJOR.MINOR.PATCH tag is required")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("An exact source commit is required")
    version = tag[1:]
    declarations = (
        ("diagnostics/src/native_plugin.c", r'#define NATIVE_VERSION "([^"]+)"'),
        ("diagnostics/native_build.sh", r"'version':'([^']+)'"),
        ("diagnostics/tools/package_native.py", r"VERSION = '([^']+)'"),
    )
    for name, pattern in declarations:
        match = re.search(pattern, (project / name).read_text())
        if not match or match[1] != version:
            raise ValueError(f"Tag does not match {name}")
    notes = project / "docs/release-notes" / f"{version}.md"
    if not notes.is_file() or not all(h in notes.read_text() for h in ("## English\n", "## 한국어\n")):
        raise ValueError("English and Korean release notes are required")
    source = artifacts / f"RE7_OTS_Native_{tag}_SteamDeck.zip"
    if not source.is_file():
        raise ValueError("Native package for this tag is missing")
    with zipfile.ZipFile(source) as z:
        names = z.namelist()
        if len(names) != len(ENTRIES) or set(names) != ENTRIES:
            raise ValueError("Unexpected, duplicate or missing installer entries")
        payload = {name: z.read(name) for name in names}
    info = json.loads(payload["BUILD_INFO.json"])
    if (info.get("schema") != 1 or info.get("version") != version or
            info.get("mode") != "ORIGINAL_NATIVE_OTS_EXPERIMENTAL" or
            info.get("commit") != commit or info.get("source_commit") != commit or
            info.get("dirty") is not False):
        raise ValueError("Package must come from this exact clean tagged build")
    dll = payload["reframework/plugins/RE7_OTS_Native.dll"]
    if info.get("dll_sha256") != hashlib.sha256(dll).hexdigest():
        raise ValueError("DLL digest does not match the build record")
    sums = {}
    for line in payload["SHA256SUMS"].decode().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match or match[2] in sums:
            raise ValueError("Invalid or duplicate package checksum")
        sums[match[2]] = match[1]
    if set(sums) != ENTRIES - {"SHA256SUMS"}:
        raise ValueError("Package checksums must cover every payload file")
    if any(hashlib.sha256(payload[name]).hexdigest() != digest for name, digest in sums.items()):
        raise ValueError("Package payload does not match its checksums")
    # No uploadable output is created before every check has succeeded.
    output = artifacts / "native-release"
    output.mkdir(exist_ok=True)
    target = output / source.name
    shutil.copyfile(source, target)
    checksums = output / "SHA256SUMS.txt"
    checksums.write_text(f"{hashlib.sha256(target.read_bytes()).hexdigest()}  {target.name}\n")
    return {"version": version, "zip": str(target), "checksums": str(checksums), "notes": str(notes)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--artifacts", type=Path, default=ROOT / "dist")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    try:
        result = prepare(ROOT, args.artifacts.resolve(), args.tag, args.commit)
    except (ValueError, OSError, zipfile.BadZipFile) as exc:
        parser.exit(1, f"Release preparation failed: {exc}\n")
    if args.github_output:
        with args.github_output.open("a") as out:
            for key, value in result.items():
                out.write(f"{key}={value}\n")
    print(json.dumps(result, indent=2))
