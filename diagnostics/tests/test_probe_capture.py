import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
REGS='rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15'.split()

class ProbeCaptureTests(unittest.TestCase):
 def test_real_detours_preserve_machine_state_and_bound_queues(self):
  # Real assembly execution, including no-capture, full and contended paths.
  with tempfile.TemporaryDirectory() as tmp:
   tmp=pathlib.Path(tmp)
   lines=['.text','.globl test_trampoline','test_trampoline:','ret',
          '.globl exercise','exercise:', 'push %rbx','push %rbp','push %r12','push %r13','push %r14','push %r15','sub $8,%rsp',
          'fxsave64 saved_fp(%rip)','fninit','fld1','fldpi']
   for i in range(16):lines += [f'movdqu test_vectors+{32*i}(%rip),%xmm{i}']
   lines += ['cmpl $0,have_avx(%rip)','je 1f']
   for i in range(16):lines += [f'vmovdqu test_vectors+{32*i}(%rip),%ymm{i}']
   lines += ['1:','fxsave64 before_fp(%rip)','pushq $0x646','popfq']
   for i,r in enumerate(REGS):lines += [f'mov ${0x120000+i},%{r}']
   lines += ['mov %rsp,before_rsp(%rip)','call *test_hook(%rip)']
   for i,r in enumerate(REGS):lines += [f'mov %{r},after_regs+{8*i}(%rip)']
   lines += ['mov %rsp,after_rsp(%rip)','pushfq','pop %rax','mov %rax,after_flags(%rip)','cld','fxsave64 after_fp(%rip)',
             'cmpl $0,have_avx(%rip)','je 2f']
   for i in range(16):lines += [f'vmovdqu %ymm{i},after_vectors+{32*i}(%rip)']
   lines += ['vzeroupper','2:','fxrstor64 saved_fp(%rip)','add $8,%rsp','pop %r15','pop %r14','pop %r13','pop %r12','pop %rbp','pop %rbx','ret',
             '.section .note.GNU-stack,"",@progbits']
   (tmp/'exercise.S').write_text('\n'.join(lines)+'\n')
   subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-no-pie','-I'+str(ROOT/'src'),
     str(ROOT/'tests/probe_capture_host.c'),str(tmp/'exercise.S'),str(ROOT/'src/probe_capture.c'),str(ROOT/'src/probe_capture.S'),'-o',str(tmp/'capture')],check=True)
   result=subprocess.run([str(tmp/'capture')],check=True,capture_output=True,text=True)
   self.assertIn('capture: all 5 detours and 4 paths PASS',result.stdout)
