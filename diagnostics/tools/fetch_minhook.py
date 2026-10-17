"""Fetch pinned MinHook build inputs; verify every used file even in a cache."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def verify(root,manifest):
 for name,digest in manifest['files'].items():
  if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
   raise ValueError('MinHook source mismatch: '+name)
def main():
 manifest=json.loads((ROOT/'data/minhook.json').read_text())
 dest=ROOT/'build/minhook'
 if not (dest/'src/hook.c').exists():
  dest.mkdir(parents=True,exist_ok=True)
  subprocess.run(['git','init',str(dest)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['git','-C',str(dest),'fetch','--depth','1',manifest['url'],manifest['commit']],check=True)
  subprocess.run(['git','-C',str(dest),'checkout','--detach','FETCH_HEAD'],check=True,stdout=subprocess.DEVNULL)
 verify(dest,manifest)
 print('MinHook pinned source verified: '+manifest['commit'])
if __name__=='__main__':main()
