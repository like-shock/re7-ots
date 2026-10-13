"""Read-only model package inventory. Filenames never prove RT/head compatibility."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import zipfile
import zlib

MAX_BYTES = 256 * 1024 * 1024
MAX_FILES = 4096
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_PATH_BYTES = 4096
MAX_DEPTH = 64
CHUNK = 64 * 1024


def normalized_path(name):
    name = name.replace('\\', '/')
    parts = name.split('/')
    if (not name or any(p in {'', '.', '..'} for p in parts)
            or any(ord(c) < 32 or c == ':' for c in name)):
        raise ValueError('unsafe entry path: ' + repr(name))
    return name


def digest(stream, limit):
    h = hashlib.sha256()
    size = 0
    while True:
        data = stream.read(min(CHUNK, limit - size + 1))
        if not data:
            return size, h.hexdigest()
        size += len(data)
        if size > limit:
            raise ValueError('package byte limit exceeded')
        h.update(data)


def walk_error(error):
    raise error


def inspect_package(source, *, max_bytes=MAX_BYTES, max_files=MAX_FILES, max_depth=MAX_DEPTH):
    """Inventory a ZIP or extracted directory; no extraction, installation or writes.

    Limits apply to all entries, not only recognized resources. The report contains
    package-relative paths and hashes, never an absolute host path or file contents.
    """
    source = Path(source)
    files, seen, nodes = [], set(), {}
    total = 0
    entries = 0

    def claim(name, is_dir):
        nonlocal entries
        entries += 1
        if entries > max_files:
            raise ValueError('package entry limit exceeded')
        # Validate before building full prefixes; those grow quadratically with depth.
        if len(name.encode('utf-8')) > MAX_PATH_BYTES:
            raise ValueError('package path length limit exceeded')
        name = normalized_path(name)
        if len(name.split('/')) > max_depth:
            raise ValueError('package path depth limit exceeded')
        key = name.casefold()
        if key in seen:
            raise ValueError('duplicate or case-ambiguous entry: ' + name)
        parts = key.split('/')
        parents = ['/'.join(parts[:i]) for i in range(1, len(parts))]
        if any(nodes.get(p) is False for p in parents) or (key in nodes and nodes[key] != is_dir):
            raise ValueError('file/directory path conflict: ' + name)
        for parent in parents:
            nodes[parent] = True
        nodes[key] = is_dir
        if len(nodes) > max_files:
            raise ValueError('package entry limit exceeded (including implicit directories)')
        seen.add(key)
        return name

    def collect(name, declared_size, opener):
        nonlocal total
        if declared_size > max_bytes - total:
            raise ValueError('package byte limit exceeded')
        with opener() as stream:
            size, sha = digest(stream, max_bytes - total)
        if size != declared_size:
            raise ValueError('entry size changed or is invalid: ' + name)
        total += size
        match = re.search(r'\.(mesh|mdf2|tex|motlist|mot|scn|pfb|user)\.(\d+)$', name, re.I)
        files.append({'path': name, 'bytes': size, 'sha256': sha,
                      'kind': match[1].lower() if match else 'other',
                      'format_suffix': match[2] if match else None})

    try:
        if source.is_symlink():
            raise ValueError('source must not be a symbolic link')
        if source.is_dir():
            provenance = {'kind': 'directory'}
            for parent, dirs, names in os.walk(source, followlinks=False, onerror=walk_error):
                for name in sorted(dirs + names):
                    path = Path(parent) / name
                    mode = path.lstat().st_mode
                    rel = claim(path.relative_to(source).as_posix(), stat.S_ISDIR(mode))
                    if stat.S_ISDIR(mode):
                        continue
                    if not stat.S_ISREG(mode):
                        raise ValueError('links and special entries are unsupported: ' + rel)
                    collect(rel, path.stat().st_size, lambda p=path: p.open('rb'))
        elif source.is_file():
            if source.suffix.lower() != '.zip':
                raise ValueError('supported input: ZIP or extracted directory; extract RAR/7z locally first')
            with source.open('rb') as raw:
                _, archive_sha = digest(raw, MAX_ARCHIVE)
                raw.seek(0)
                provenance = {'kind': 'zip', 'sha256': archive_sha}
                with zipfile.ZipFile(raw) as archive:
                    for item in archive.infolist():
                        # ZipInfo may truncate names at NUL; inspect its original name.
                        name = claim(item.orig_filename[:-1] if item.is_dir() else item.orig_filename, item.is_dir())
                        mode = item.external_attr >> 16
                        kind = stat.S_IFMT(mode)
                        if kind not in {0, stat.S_IFREG, stat.S_IFDIR}:
                            raise ValueError('links and special entries are unsupported: ' + name)
                        if item.is_dir():
                            if kind == stat.S_IFREG or item.file_size:
                                raise ValueError('invalid directory entry: ' + name)
                            continue
                        if kind == stat.S_IFDIR:
                            raise ValueError('invalid file entry: ' + name)
                        collect(name, item.file_size, lambda i=item: archive.open(i))
        else:
            raise ValueError('input does not exist or is not a file/directory')
    except (OSError, zipfile.BadZipFile, RuntimeError, NotImplementedError, EOFError, UnicodeError, zlib.error) as exc:
        raise ValueError('cannot read model package: ' + str(exc)) from exc
    if not files:
        raise ValueError('package has no files')
    return {'schema': 1, 'source': provenance, 'compatibility': 'UNVERIFIED',
            'file_count': len(files), 'total_bytes': total,
            'files': sorted(files, key=lambda f: f['path'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='ZIP archive or extracted model directory')
    args = parser.parse_args()
    try:
        report = inspect_package(args.source)
    except ValueError as exc:
        print('Model inventory failed: ' + str(exc), file=sys.stderr)
        return 2
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
