import pathlib, platform, re, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
REGS='rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15'.split()
class NativeBridgeTests(unittest.TestCase):
 def test_native_callback_preserves_processor_context_and_original_or_handled_exit(self):
  with tempfile.TemporaryDirectory() as tmp:
   tmp=pathlib.Path(tmp)
   lines=['.text','.globl test_original','test_original:','movl $1,exit_path(%rip)','jmp test_resume',
          '.globl test_handled','test_handled:','movl $2,exit_path(%rip)','jmp test_resume',
          '.globl exercise','exercise:','push %rbx','push %rbp','push %r12','push %r13','push %r14','push %r15','sub $8,%rsp',
          'sub $16,%rsp','mov game_stack_padding(%rip),%eax','sub %rax,%rsp',
          'movabs $0xBADBADBADBADBAD,%rax','mov %rax,(%rsp)',
          'fxsave64 saved_fp(%rip)','fninit','fld1','fldpi','fld1','fldpi','fld1','fldpi','fld1','fldpi']
   for i in range(16):lines += [f'movdqu test_vectors+{32*i}(%rip),%xmm{i}']
   lines += ['cmpl $0,have_avx(%rip)','je 1f']
   for i in range(16):lines += [f'vmovdqu test_vectors+{32*i}(%rip),%ymm{i}']
   lines += ['1:','fxsave64 before_fp(%rip)','pushq $0x646','popfq']
   for i,r in enumerate(REGS):lines += [f'mov ${0x120000+i},%{r}']
   lines += ['mov %rsp,before_rsp(%rip)','jmp *test_hook(%rip)','.globl test_resume','test_resume:']
   for i,r in enumerate(REGS):lines += [f'mov %{r},after_regs+{8*i}(%rip)']
   lines += ['mov %rsp,after_rsp(%rip)','pushfq','pop %rax','mov %rax,after_flags(%rip)','cld','fxsave64 after_fp(%rip)',
             'cmpl $0,have_avx(%rip)','je 2f']
   for i in range(16):lines += [f'vmovdqu %ymm{i},after_vectors+{32*i}(%rip)']
   lines += ['vzeroupper','2:','fxrstor64 saved_fp(%rip)','mov game_stack_padding(%rip),%eax','add %rax,%rsp','add $16,%rsp','add $8,%rsp','pop %r15','pop %r14','pop %r13','pop %r12','pop %rbp','pop %rbx','ret','.section .note.GNU-stack,"",@progbits']
   bridge=ROOT/'src/native_bridge.S';flags=['-no-pie']
   if platform.system()=='Darwin':
    flags=['-arch','x86_64']
    def macho(source):
     source=re.sub(r'^\.section \.note.GNU-stack.*$', '', source, flags=re.M)
     return re.sub(r'\b(?:native_\w+|test_\w+|after_\w+|before_\w+|saved_fp|have_avx|exit_path|game_stack_padding|exercise)\b',lambda m:'_'+m[0],source)
    lines=macho('\n'.join(lines)).splitlines()
    source=subprocess.run(['cc','-E','-P','-x','assembler-with-cpp','-I'+str(ROOT/'src'),str(bridge)],check=True,capture_output=True,text=True).stdout
    bridge=tmp/'bridge.S';bridge.write_text(macho(source))
   (tmp/'exercise.S').write_text('\n'.join(lines)+'\n')
   subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'src'),str(ROOT/'tests/native_bridge_host.c'),str(tmp/'exercise.S'),str(ROOT/'src/native_bridge.c'),str(bridge),'-o',str(tmp/'test')],check=True)
   p=subprocess.run([str(tmp/'test')],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stderr)
   self.assertIn('native bridge: PASS',p.stdout)
