import hashlib,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
OLD=['reframework/plugins/RE7_OTS_Explore.dll','reframework/autorun/re7_ots_explore.lua','reframework/plugins/RE7_OTS_Observer.dll','reframework/plugins/RE7_OTS_Diagnostics.dll']
class ProbeInstallTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.pkg=pathlib.Path(self.tmp.name)/'package';self.game=pathlib.Path(self.tmp.name)/'game with spaces'
  self.pkg.mkdir();self.game.mkdir();(self.game/'re7.exe').write_bytes(b'game')
  for name in ('install.sh','uninstall.sh'):shutil.copyfile(ROOT/'probe'/name,self.pkg/name)
  self.dll='reframework/plugins/RE7_OTS_NativeProbe.dll'
  (self.pkg/self.dll).parent.mkdir(parents=True);(self.pkg/self.dll).write_bytes(b'new probe')
  (self.pkg/'SHA256SUMS').write_text(hashlib.sha256(b'new probe').hexdigest()+'  '+self.dll+'\n')
  for name in OLD:
   p=self.game/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(name.encode())
 def run_script(self,name,ok=True):
  p=subprocess.run(['bash',str(self.pkg/name),str(self.game)],capture_output=True,text=True)
  self.assertEqual(p.returncode==0,ok,p.stdout+p.stderr);return p
 def test_install_backup_restore_with_spaces_and_logs_preserved(self):
  self.run_script('install.sh')
  for name in OLD:self.assertFalse((self.game/name).exists())
  self.assertEqual((self.game/self.dll).read_bytes(),b'new probe')
  log=self.game/'reframework/data/RE7_OTS_NativeProbe/keep.jsonl';log.write_text('evidence')
  self.run_script('uninstall.sh')
  self.assertFalse((self.game/self.dll).exists());self.assertEqual(log.read_text(),'evidence')
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
 def test_corrupt_payload_is_rejected_before_moving_old_files(self):
  (self.pkg/self.dll).write_bytes(b'corrupt');self.run_script('install.sh',False)
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
 def test_second_install_and_changed_probe_are_not_overwritten(self):
  self.run_script('install.sh');self.run_script('install.sh',False)
  (self.game/self.dll).write_bytes(b'user replacement');self.run_script('uninstall.sh',False)
  self.assertEqual((self.game/self.dll).read_bytes(),b'user replacement')
 def test_uninstall_rejects_conflicting_restoration_and_changed_backup(self):
  self.run_script('install.sh');(self.game/OLD[0]).write_bytes(b'other work')
  self.run_script('uninstall.sh',False);self.assertEqual((self.game/OLD[0]).read_bytes(),b'other work')
  (self.game/OLD[0]).unlink()
  backup=self.game/'reframework/data/RE7_OTS_NativeProbe/install/backup'/OLD[0]
  backup.write_bytes(b'changed backup');self.run_script('uninstall.sh',False)
  self.assertTrue((self.game/self.dll).exists())

 def test_failed_copy_rolls_back_all_backups(self):
  binpath=pathlib.Path(self.tmp.name)/'bin';binpath.mkdir()
  cp=binpath/'cp';cp.write_text('#!/bin/sh\nexit 9\n');cp.chmod(0o755)
  env=dict(os.environ,PATH=str(binpath)+os.pathsep+os.environ['PATH'])
  p=subprocess.run(['bash',str(self.pkg/'install.sh'),str(self.game)],env=env,capture_output=True,text=True)
  self.assertNotEqual(p.returncode,0)
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
  self.assertFalse((self.game/self.dll).exists())
 def test_failed_restore_rolls_back_partial_moves(self):
  self.run_script('install.sh')
  binpath=pathlib.Path(self.tmp.name)/'bin';binpath.mkdir();counter=binpath/'count'
  mv=binpath/'mv';mv.write_text('#!/bin/bash\nn=0; [[ ! -f "'+str(counter)+'" ]] || n=$(cat "'+str(counter)+'")\nn=$((n+1)); echo "$n" > "'+str(counter)+'"\n[[ "$n" != 2 ]] || exit 9\nexec /bin/mv "$@"\n');mv.chmod(0o755)
  env=dict(os.environ,PATH=str(binpath)+os.pathsep+os.environ['PATH'])
  p=subprocess.run(['bash',str(self.pkg/'uninstall.sh'),str(self.game)],env=env,capture_output=True,text=True)
  self.assertNotEqual(p.returncode,0);self.assertTrue((self.game/self.dll).exists())
  for name in OLD:self.assertFalse((self.game/name).exists())
  self.run_script('uninstall.sh')

class ProbeArchiveTests(unittest.TestCase):
 def test_archive_payload_provenance_and_source_tampering(self):
  import importlib.util,json,sys,zipfile
  sys.path.insert(0,str(ROOT/'tools'))
  self.addCleanup(lambda:sys.path.remove(str(ROOT/'tools')))
  spec=importlib.util.spec_from_file_location('package_probe',ROOT/'tools/package_probe.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  with tempfile.TemporaryDirectory() as d:
   project=pathlib.Path(d)/'repo';diag=project/'diagnostics';diag.mkdir(parents=True)
   shutil.copytree(ROOT/'probe',diag/'probe')
   for name in ('MINHOOK_LICENSE.txt','NOTICE.md','ORIGINAL_CREDITS.txt','LICENSE'):shutil.copyfile(ROOT/name,diag/name)
   (diag/'tools').mkdir();shutil.copyfile(ROOT/'tools/summarize_probe.py',diag/'tools/summarize_probe.py')
   (project/'.gitignore').write_text('diagnostics/build/\n')
   subprocess.run(['git','init','-q',str(project)],check=True)
   subprocess.run(['git','-C',str(project),'add','.'],check=True)
   subprocess.run(['git','-C',str(project),'-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','fixture'],check=True)
   sha=subprocess.check_output(['git','-C',str(project),'rev-parse','HEAD'],text=True).strip()
   build=diag/'build';build.mkdir();(build/'RE7_OTS_NativeProbe.dll').write_bytes(b'fixture DLL')
   info={'commit':sha,'dirty':False,'dll_sha256':hashlib.sha256(b'fixture DLL').hexdigest(),'sources':{'probe/install.sh':hashlib.sha256((diag/'probe/install.sh').read_bytes()).hexdigest()}}
   (build/'probe-build.json').write_text(json.dumps(info))
   target=m.package(project,pathlib.Path(d)/'out')
   with zipfile.ZipFile(target) as z:
    self.assertFalse(any('Explore.dll' in p or p.endswith('.lua') for p in z.namelist()))
    for entry in z.read('SHA256SUMS').decode().splitlines():
     digest,name=entry.split('  ',1);self.assertEqual(digest,hashlib.sha256(z.read(name)).hexdigest())
    self.assertEqual(json.loads(z.read('BUILD_INFO.json'))['commit'],sha)
   info['sources']['probe/install.sh']='0'*64;(build/'probe-build.json').write_text(json.dumps(info))
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')
