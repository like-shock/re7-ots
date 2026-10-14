"""Package committed Explore sources and verified generated Lua; run test.sh first."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

from build_explore_lua import render


def sums(items):
    return ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n'
                   for name, data in sorted(items.items())).encode('utf-8')


def archive(path, items):
    # Fixed metadata makes identical inputs byte-identical within the same toolchain.
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.package-', delete=False) as tmp:
        pending = Path(tmp.name)
    try:
        with ZipFile(pending, 'w') as z:
            for name, data in sorted(items.items()):
                info = ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                z.writestr(info, data, compress_type=ZIP_DEFLATED, compresslevel=9)
        with ZipFile(pending) as z:
            if z.testzip() is not None or set(z.namelist()) != set(items):
                raise ValueError('ZIP CRC/entry verification failed')
            for name, data in items.items():
                if z.read(name) != data:
                    raise ValueError('ZIP payload verification failed: ' + name)
        os.replace(pending, path)
    finally:
        pending.unlink(missing_ok=True)
    print(f'{path.name}: {len(items)} files, {path.stat().st_size} bytes, CRC/payload PASS')


def package(project, out):
    diag = project/'diagnostics'

    def git(*args):
        return subprocess.check_output(['git', '-C', str(project), *args], stderr=subprocess.PIPE).decode('utf-8').strip()

    if git('status', '--porcelain', '--untracked-files=no'):
        raise ValueError('uncommitted tracked changes; commit the tested sources before packaging')
    adapter = (diag/'lua/explore/adapter.lua').read_text(encoding='utf-8')
    match = re.search(r"^local VERSION='([0-9A-Za-z.+-]+)'$", adapter, re.MULTILINE)
    if not match:
        raise ValueError('cannot determine Explore version')
    version = match[1]
    lua = (diag/'build/re7_ots_explore.lua').read_bytes()
    if lua != render(diag).encode('utf-8'):
        raise ValueError('generated Lua content/version differs from source; run test.sh again')
    build_info = (json.dumps({'schema': 1, 'version': version,
        'commit': git('rev-parse', 'HEAD'), 'tree': git('rev-parse', 'HEAD^{tree}')},
        indent=2, sort_keys=True) + '\n').encode('utf-8')
    installer = {
        'reframework/plugins/RE7_OTS_Explore.dll': (diag/'build/RE7_OTS_Explore.dll').read_bytes(),
        'reframework/autorun/re7_ots_explore.lua': lua,
        'README_KO.md': (diag/'EXPLORE_README_KO.md').read_bytes(),
        'LICENSE': (diag/'LICENSE').read_bytes(),
        'NOTICE.md': (diag/'NOTICE.md').read_bytes(),
        'ORIGINAL_CREDITS.txt': (diag/'ORIGINAL_CREDITS.txt').read_bytes(),
        'BUILD_INFO.json': build_info,
    }
    installer['SHA256SUMS'] = sums(installer)
    source = {}
    tracked = subprocess.check_output(['git', '-C', str(project), 'ls-files', '-z']).decode('utf-8').split('\0')
    for name in sorted(n for n in tracked if n):
        rel = Path(name)
        if any(x in {'.git', '__pycache__', 'build'} for x in rel.parts):
            continue
        if rel.parts[0] in {'releases', 'dist'} or name in {'SHA256SUMS', 'BUILD_INFO.json'}:
            continue
        source[name] = (project/rel).read_bytes()
    source['BUILD_INFO.json'] = build_info
    source['SHA256SUMS'] = sums(source)
    # Read and validate all inputs before replacing any existing deliverable.
    out.mkdir(parents=True, exist_ok=True)
    archive(out/f'RE7_OTS_Explore_v{version}_SteamDeck.zip', installer)
    prefix = f'RE7_OTS_Explore_v{version}/'
    archive(out/f'RE7_OTS_Explore_v{version}_Source.zip',
            {prefix+name: data for name, data in source.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        package(Path(__file__).resolve().parents[2], args.output.resolve())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print('Packaging failed: ' + str(exc), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
