"""Fetch the official, hash-pinned Lua core used by the REFramework reference.

Build cache only; no executables or archive paths are trusted before verification.
"""
import hashlib
import io
from pathlib import Path
import tarfile
import urllib.request

VERSION = '5.4.3'
SHA256 = 'f8612276169e3bfcbcfb8f226195bfc6e466fe13042f1076cbde92b7ec96bbfb'
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'build' / ('lua-' + VERSION)

def fetch():
    archive = ROOT / 'build' / ('lua-' + VERSION + '.tar.gz')
    data = archive.read_bytes() if archive.exists() else b''
    if hashlib.sha256(data).hexdigest() != SHA256:
        with urllib.request.urlopen('https://www.lua.org/ftp/lua-' + VERSION + '.tar.gz', timeout=60) as response:
            data = response.read(1024 * 1024)
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise RuntimeError('Lua source hash mismatch')
        archive.parent.mkdir(parents=True, exist_ok=True)
        archive.write_bytes(data)
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as tar:
        for member in tar.getmembers():
            path = Path(member.name)
            if (member.isfile() and len(path.parts) == 3 and path.parts[0] == 'lua-' + VERSION
                    and path.parts[1] == 'src' and path.suffix in ('.c', '.h')):
                target = DEST / 'src' / path.name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(tar.extractfile(member).read())
    return DEST

if __name__ == '__main__':
    print(fetch())
