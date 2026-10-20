import hashlib,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
OLD=['reframework/plugins/RE7_OTS_Explore.dll','reframework/autorun/re7_ots_explore.lua','reframework/plugins/RE7_OTS_Observer.dll','reframework/plugins/RE7_OTS_Diagnostics.dll','reframework/plugins/RE7_OTS_NativeProbe.dll']
def seed_minhook(diag):
 import json
 content=b'fixture pinned MinHook source'
 manifest={'commit':'a'*40,'files':{'src/hook.c':hashlib.sha256(content).hexdigest()}}
 (diag/'data').mkdir();(diag/'data/minhook.json').write_text(json.dumps(manifest))
 cache=diag/'build/minhook/src';cache.mkdir(parents=True);(cache/'hook.c').write_bytes(content)
 return manifest
def seed_lua(diag):
 import importlib.util
 spec=importlib.util.spec_from_file_location('native_fetch_lua',ROOT/'tools/fetch_lua.py')
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 shutil.copyfile(ROOT/'build/lua-5.4.3.tar.gz',diag/'build/lua-5.4.3.tar.gz')
 shutil.copytree(ROOT/'build/lua-5.4.3',diag/'build/lua-5.4.3')
 return {'lua_version':module.VERSION,'lua_archive_sha256':module.SHA256,
         'lua_sources':module.source_hashes(diag/'build')}
class NativeInstallTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.pkg=pathlib.Path(self.tmp.name)/'package';self.game=pathlib.Path(self.tmp.name)/'game with spaces'
  self.pkg.mkdir();self.game.mkdir();(self.game/'re7.exe').write_bytes(b'game')
  for name in ('install.sh','uninstall.sh'):shutil.copyfile(ROOT/'native'/name,self.pkg/name)
  self.dll='reframework/plugins/RE7_OTS_Native.dll'
  (self.pkg/self.dll).parent.mkdir(parents=True);(self.pkg/self.dll).write_bytes(b'new native')
  (self.pkg/'SHA256SUMS').write_text(hashlib.sha256(b'new native').hexdigest()+'  '+self.dll+'\n')
  for name in OLD:
   p=self.game/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(name.encode())
 def run_script(self,name,ok=True):
  p=subprocess.run(['bash',str(self.pkg/name),str(self.game)],capture_output=True,text=True)
  self.assertEqual(p.returncode==0,ok,p.stdout+p.stderr);return p
 def test_install_backup_restore_with_spaces_and_logs_preserved(self):
  model=self.game/'natives/x64/sectionroot/character/existing_model.mesh';model.parent.mkdir(parents=True);model.write_bytes(b'user model')
  self.run_script('install.sh')
  for name in OLD:self.assertFalse((self.game/name).exists())
  self.assertEqual((self.game/self.dll).read_bytes(),b'new native')
  log=self.game/'reframework/data/RE7_OTS_Native/keep.jsonl';log.write_text('evidence')
  config=log.parent/'settings.cfg';config.write_text('auto_disable_with_melee_weapons=0\n')
  self.run_script('uninstall.sh')
  self.assertFalse((self.game/self.dll).exists());self.assertEqual(log.read_text(),'evidence')
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
  self.assertEqual(model.read_bytes(),b'user model')
  self.assertEqual(config.read_text(),'auto_disable_with_melee_weapons=0\n')
 def test_corrupt_payload_is_rejected_before_moving_old_files(self):
  (self.pkg/self.dll).write_bytes(b'corrupt');self.run_script('install.sh',False)
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
 def test_second_install_and_changed_native_are_not_overwritten(self):
  self.run_script('install.sh');self.run_script('install.sh',False)
  (self.game/self.dll).write_bytes(b'user replacement');self.run_script('uninstall.sh',False)
  self.assertEqual((self.game/self.dll).read_bytes(),b'user replacement')
 def test_uninstall_rejects_conflicting_restoration_and_changed_backup(self):
  self.run_script('install.sh');(self.game/OLD[0]).write_bytes(b'other work')
  self.run_script('uninstall.sh',False);self.assertEqual((self.game/OLD[0]).read_bytes(),b'other work')
  (self.game/OLD[0]).unlink()
  backup=self.game/'reframework/data/RE7_OTS_Native/install/backup'/OLD[0]
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

 def test_running_game_blocks_install_and_restore(self):
  binpath=pathlib.Path(self.tmp.name)/'running';binpath.mkdir()
  pgrep=binpath/'pgrep';pgrep.write_text('#!/bin/sh\nexit 0\n');pgrep.chmod(0o755)
  env=dict(os.environ,PATH=str(binpath)+os.pathsep+os.environ['PATH'])
  p=subprocess.run(['bash',str(self.pkg/'install.sh'),str(self.game)],env=env,capture_output=True,text=True)
  self.assertNotEqual(p.returncode,0)
  for name in OLD:self.assertEqual((self.game/name).read_bytes(),name.encode())
  self.run_script('install.sh')
  p=subprocess.run(['bash',str(self.pkg/'uninstall.sh'),str(self.game)],env=env,capture_output=True,text=True)
  self.assertNotEqual(p.returncode,0);self.assertTrue((self.game/self.dll).exists())

 def test_existing_native_and_symlinked_old_plugin_are_rejected(self):
  installed=self.game/self.dll;installed.write_bytes(b'existing native')
  self.run_script('install.sh',False)
  self.assertEqual(installed.read_bytes(),b'existing native')
  installed.unlink()
  old=self.game/OLD[-1];old.unlink();old.symlink_to(self.game/OLD[0])
  self.run_script('install.sh',False)
  self.assertTrue(old.is_symlink())

class NativeArchiveTests(unittest.TestCase):
 def test_archive_payload_provenance_and_source_tampering(self):
  import importlib.util,json,sys,zipfile
  sys.path.insert(0,str(ROOT/'tools'))
  self.addCleanup(lambda:sys.path.remove(str(ROOT/'tools')))
  spec=importlib.util.spec_from_file_location('package_native',ROOT/'tools/package_native.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  with tempfile.TemporaryDirectory() as d:
   project=pathlib.Path(d)/'repo';diag=project/'diagnostics';diag.mkdir(parents=True)
   shutil.copytree(ROOT/'native',diag/'native')
   for name in ('MINHOOK_LICENSE.txt','NOTICE.md','ORIGINAL_CREDITS.txt','LICENSE'):shutil.copyfile(ROOT/name,diag/name)
   pin=seed_minhook(diag);lua=seed_lua(diag)
   (project/'.gitignore').write_text('diagnostics/build/\n')
   subprocess.run(['git','init','-q',str(project)],check=True)
   subprocess.run(['git','-C',str(project),'add','.'],check=True)
   subprocess.run(['git','-C',str(project),'-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','fixture'],check=True)
   sha=subprocess.check_output(['git','-C',str(project),'rev-parse','HEAD'],text=True).strip()
   build=diag/'build';build.mkdir(exist_ok=True);(build/'RE7_OTS_Native.dll').write_bytes(b'fixture DLL')
   info={'schema':1,'version':'0.1.16','mode':'ORIGINAL_NATIVE_OTS_EXPERIMENTAL','commit':sha,'dirty':False,'dll_sha256':hashlib.sha256(b'fixture DLL').hexdigest(),
         'sources':{name:hashlib.sha256((diag/name).read_bytes()).hexdigest() for name in ('native/install.sh','data/minhook.json')},
         'minhook_commit':pin['commit'],'minhook_sources':pin['files'],**lua}
   (build/'native-build.json').write_text(json.dumps(info))
   target=m.package(project,pathlib.Path(d)/'out')
   self.assertEqual(target.name,'RE7_OTS_Native_v0.1.16_SteamDeck.zip')
   with zipfile.ZipFile(target) as z:
    self.assertTrue({'README_KO.md','README_EN.md'} <= set(z.namelist()))
    self.assertFalse(any('Explore.dll' in p or p.endswith('.lua') for p in z.namelist()))
    self.assertEqual([p for p in z.namelist() if p.endswith('.dll')],['reframework/plugins/RE7_OTS_Native.dll'])
    self.assertFalse(any(p.startswith('natives/') or p.lower().endswith(('.pak','.ct','.mesh')) for p in z.namelist()))
    self.assertEqual(json.loads(z.read('BUILD_INFO.json'))['mode'],'ORIGINAL_NATIVE_OTS_EXPERIMENTAL')
    for entry in z.read('SHA256SUMS').decode().splitlines():
     digest,name=entry.split('  ',1);self.assertEqual(digest,hashlib.sha256(z.read(name)).hexdigest())
    self.assertEqual(json.loads(z.read('BUILD_INFO.json'))['commit'],sha)
   info['sources']['native/install.sh']='0'*64;(build/'native-build.json').write_text(json.dumps(info))
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')

 def test_manifest_mode_revision_binary_and_source_provenance_are_enforced(self):
  import importlib.util,json,sys
  sys.path.insert(0,str(ROOT/'tools'));self.addCleanup(lambda:sys.path.remove(str(ROOT/'tools')))
  spec=importlib.util.spec_from_file_location('package_native',ROOT/'tools/package_native.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  with tempfile.TemporaryDirectory() as d:
   project=pathlib.Path(d)/'repo';diag=project/'diagnostics';diag.mkdir(parents=True)
   shutil.copytree(ROOT/'native',diag/'native')
   for name in ('MINHOOK_LICENSE.txt','NOTICE.md','ORIGINAL_CREDITS.txt','LICENSE'):shutil.copyfile(ROOT/name,diag/name)
   pin=seed_minhook(diag);lua=seed_lua(diag)
   (project/'.gitignore').write_text('diagnostics/build/\n')
   subprocess.run(['git','init','-q',str(project)],check=True)
   subprocess.run(['git','-C',str(project),'add','.'],check=True)
   subprocess.run(['git','-C',str(project),'-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','fixture'],check=True)
   sha=subprocess.check_output(['git','-C',str(project),'rev-parse','HEAD'],text=True).strip()
   build=diag/'build';build.mkdir(exist_ok=True);dll=build/'RE7_OTS_Native.dll';dll.write_bytes(b'fixture DLL')
   good={'schema':1,'version':'0.1.16','mode':'ORIGINAL_NATIVE_OTS_EXPERIMENTAL','commit':sha,'dirty':False,
         'dll_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),
         'sources':{name:hashlib.sha256((diag/name).read_bytes()).hexdigest() for name in ('native/install.sh','data/minhook.json')},
         'minhook_commit':pin['commit'],'minhook_sources':pin['files'],**lua}
   mutations=[{'schema':2},{'version':'9.9.9'},{'mode':'PASS_THROUGH_HOOK_PROBE'},
              {'commit':'0'*40},{'dirty':True},{'dll_sha256':'0'*64},{'sources':{}},
              {'minhook_commit':'b'*40},{'minhook_sources':{}},
              {'lua_version':'5.3'},{'lua_archive_sha256':'0'*64},{'lua_sources':{}},
              {'minhook_sources':{'src/hook.c':'0'*64}},
              {'sources':{'../.gitignore':hashlib.sha256((project/'.gitignore').read_bytes()).hexdigest()}}]
   for change in mutations:
    with self.subTest(change=change):
     (build/'native-build.json').write_text(json.dumps(good|change))
     with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')
   untracked=diag/'untracked.c';untracked.write_text('untracked source')
   (build/'native-build.json').write_text(json.dumps(good|{'sources':{'untracked.c':hashlib.sha256(untracked.read_bytes()).hexdigest()}}))
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')
   (build/'native-build.json').write_text(json.dumps(good))
   lua_source=build/'lua-5.4.3/src/lapi.c';original_lua=lua_source.read_bytes()
   lua_source.write_bytes(b'changed Lua core')
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad-lua')
   lua_source.write_bytes(original_lua)
   (build/'minhook/src/hook.c').write_bytes(b'tampered cache')
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')
   (diag/'native/install.sh').write_text('dirty source')
   with self.assertRaises(ValueError):m.package(project,pathlib.Path(d)/'bad')
   self.assertFalse((pathlib.Path(d)/'bad').exists())
