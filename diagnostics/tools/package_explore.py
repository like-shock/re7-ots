"""Build and verify Explore installer/source archives from the working tree."""
import hashlib
import subprocess
import sys
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

diag = Path(__file__).resolve().parents[1]
project = diag.parent
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
version = '0.4.0-dev'


def sums(items):
    return ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n'
                   for name, data in sorted(items.items())).encode()


def archive(path, items):
    with ZipFile(path, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(items.items()):
            z.writestr(name, data)
    with ZipFile(path) as z:
        assert z.testzip() is None, 'ZIP CRC failure'
        assert set(z.namelist()) == set(items), 'ZIP entry mismatch'
        for name, data in items.items():
            assert z.read(name) == data, name
    print(f'{path.name}: {len(items)} files, {path.stat().st_size} bytes, CRC/payload PASS')


installer = {
    'reframework/plugins/RE7_OTS_Explore.dll': (diag/'build/RE7_OTS_Explore.dll').read_bytes(),
    'reframework/autorun/re7_ots_explore.lua': (diag/'build/re7_ots_explore.lua').read_bytes(),
    'README_KO.md': (diag/'EXPLORE_README_KO.md').read_bytes(),
    'LICENSE': (diag/'LICENSE').read_bytes(),
    'NOTICE.md': (diag/'NOTICE.md').read_bytes(),
    'ORIGINAL_CREDITS.txt': (diag/'ORIGINAL_CREDITS.txt').read_bytes(),
}
installer['SHA256SUMS'] = sums(installer)
archive(out/f'RE7_OTS_Explore_v{version}_SteamDeck.zip', installer)

source = {}
tracked = subprocess.check_output(['git', '-C', str(project), 'ls-files', '-z']).decode().split('\0')
for name in sorted(n for n in tracked if n):
    p = project/name
    rel = p.relative_to(project)
    if any(x in {'.git', '__pycache__', 'build'} for x in rel.parts):
        continue
    if rel.parts[0] == 'releases' or rel.as_posix() == 'SHA256SUMS':
        continue
    source[rel.as_posix()] = p.read_bytes()
source['SHA256SUMS'] = sums(source)
prefix = f'RE7_OTS_Explore_v{version}/'
archive(out/f'RE7_OTS_Explore_v{version}_Source.zip',
        {prefix+name: data for name, data in source.items()})
