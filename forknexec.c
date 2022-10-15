#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
forknexec(const char *path, const char **args)
{
  //fork
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  //exec구성
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;


  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -2; //실행 에러 리턴2
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }




  // Clear %eax so that fork returns 0 in the child.
//  np->tf->eax = 0;


/*
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
*/
  np->cwd = idup(curproc->cwd);



//  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
/** 옮김
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
**/

//여기부터 exec


  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
//  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      return -1; //arg에러는 1리턴
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
//  ustack[3+argc] = 0;

//  ustack[0] = 0xffffffff;  // fake return PC
//  ustack[1] = argc;
//  ustack[2] = sp - (argc+1)*4;  // argv pointer

//  sp -= (3+argc+1) * 4;
//  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
//    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(np->name, last, sizeof(np->name));

  // Commit to the user image.
//  oldpgdir = curproc->pgdir;
  np->pgdir = pgdir;
  np->sz = sz;
  np->tf->eip = elf.entry;  // main
  np->tf->esp = sp;
//  switchuvm(curproc);
//  freevm(oldpgdir);

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return np->pid;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -2;
}
