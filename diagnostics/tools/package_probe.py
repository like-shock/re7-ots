"""Package the separately labeled original-hook device probe from a clean build."""
import argparse,hashlib,json,pathlib,subprocess
from package_explore import archive,sums
ROOT=pathlib.Path(__file__).resolve().parents[2]

def package(project,out):
 diag=project/'diagnostics'
 def git(*args):return subprocess.check_output(['git','-C',str(project),*args],text=True).strip()
 if git('status','--porcelain','--untracked-files=no'):raise ValueError('Commit tracked changes before packaging')
 info=json.loads((diag/'build/probe-build.json').read_text())
 dll=(diag/'build/RE7_OTS_NativeProbe.dll').read_bytes()
 if info['commit']!=git('rev-parse','HEAD') or info['dirty'] or info['dll_sha256']!=hashlib.sha256(dll).hexdigest():raise ValueError('Probe build does not match clean HEAD; rebuild')
 for name,digest in info['sources'].items():
  if hashlib.sha256((diag/name).read_bytes()).hexdigest()!=digest:raise ValueError('Probe build source changed: '+name)
 items={'reframework/plugins/RE7_OTS_NativeProbe.dll':dll,
        'BUILD_INFO.json':(json.dumps(info,indent=2,sort_keys=True)+'\n').encode()}
 for name in ('install.sh','uninstall.sh','README_KO.md'):items[name]=(diag/'probe'/name).read_bytes()
 for name in ('MINHOOK_LICENSE.txt','NOTICE.md','ORIGINAL_CREDITS.txt','LICENSE'):items[name]=(diag/name).read_bytes()
 items['summarize_probe.py']=(diag/'tools/summarize_probe.py').read_bytes()
 items['SHA256SUMS']=sums(items);out.mkdir(parents=True,exist_ok=True)
 target=out/'RE7_OTS_NativeProbe_v0.1.0_SteamDeck.zip';archive(target,items);return target

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
 try:package(ROOT,a.output.resolve())
 except (ValueError,OSError,subprocess.CalledProcessError) as exc:p.exit(2,str(exc)+'\n')
