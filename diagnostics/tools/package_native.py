"""Package the experimental original native OTS gameplay build from clean HEAD."""
import argparse
import hashlib
import json
import pathlib
import subprocess

from package_explore import archive, sums
import fetch_lua

ROOT = pathlib.Path(__file__).resolve().parents[2]
VERSION = '0.1.13'
MODE = 'ORIGINAL_NATIVE_OTS_EXPERIMENTAL'


def package(project, out):
    diag = project/'diagnostics'

    def git(*args):
        return subprocess.check_output(['git', '-C', str(project), *args],
                                       stderr=subprocess.PIPE)

    if git('status', '--porcelain', '--untracked-files=no').strip():
        raise ValueError('Commit tracked changes before packaging')
    info = json.loads((diag/'build/native-build.json').read_text())
    if (info.get('schema') != 1 or info.get('version') != VERSION or
            info.get('mode') != MODE):
        raise ValueError('Native build schema/version/mode mismatch; rebuild')
    dll = (diag/'build/RE7_OTS_Native.dll').read_bytes()
    if (info.get('commit') != git('rev-parse', 'HEAD').decode().strip() or
            info.get('dirty') is not False or
            info.get('dll_sha256') != hashlib.sha256(dll).hexdigest()):
        raise ValueError('Native build does not match clean HEAD; rebuild')
    sources = info.get('sources')
    if not isinstance(sources, dict) or not sources or 'data/minhook.json' not in sources:
        raise ValueError('Native build source manifest missing')
    for name, digest in sources.items():
        path = pathlib.PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or str(path) != name or '\\' in name:
            raise ValueError('Invalid Native build source path: ' + name)
        current = (diag/name).read_bytes()
        try:
            committed = git('show', 'HEAD:diagnostics/' + name)
        except subprocess.CalledProcessError as exc:
            raise ValueError('Native build source is not in HEAD: ' + name) from exc
        if current != committed or hashlib.sha256(current).hexdigest() != digest:
            raise ValueError('Native build source changed: ' + name)
    # External cache files are authorized by the tracked pin, not arbitrary
    # ignored paths supplied by the build manifest. Verify their actual bytes.
    pin = json.loads((diag/'data/minhook.json').read_text())
    if (info.get('minhook_commit') != pin['commit'] or
            info.get('minhook_sources') != pin['files']):
        raise ValueError('Native build MinHook provenance differs from tracked pin')
    for name, digest in pin['files'].items():
        path = pathlib.PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or str(path) != name or '\\' in name:
            raise ValueError('Invalid pinned MinHook source path: ' + name)
        if hashlib.sha256((diag/'build/minhook'/name).read_bytes()).hexdigest() != digest:
            raise ValueError('Pinned MinHook cache changed: ' + name)
    if (info.get('lua_version') != fetch_lua.VERSION or
            info.get('lua_archive_sha256') != fetch_lua.SHA256 or
            info.get('lua_sources') != fetch_lua.source_hashes(diag/'build')):
        raise ValueError('Native build Lua provenance differs from pinned source')
    items = {
        'reframework/plugins/RE7_OTS_Native.dll': dll,
        'BUILD_INFO.json': (json.dumps(info, indent=2, sort_keys=True)+'\n').encode(),
    }
    for name in ('install.sh', 'uninstall.sh', 'README_KO.md'):
        items[name] = (diag/'native'/name).read_bytes()
    for name in ('MINHOOK_LICENSE.txt', 'NOTICE.md', 'ORIGINAL_CREDITS.txt', 'LICENSE'):
        items[name] = (diag/name).read_bytes()
    items['SHA256SUMS'] = sums(items)
    out.mkdir(parents=True, exist_ok=True)
    target = out/f'RE7_OTS_Native_v{VERSION}_SteamDeck.zip'
    archive(target, items)
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    try:
        package(ROOT, args.output.resolve())
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        parser.exit(2, str(exc)+'\n')
