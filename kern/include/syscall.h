/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include<types.h>
#include<synch.h>
#include<kern/limits.h>
#include <cdefs.h> /* for __DEAD */
struct trapframe; /* from <machine/trapframe.h> */
struct proc *proc_ids[__PID_MAX];
struct lock *argsmallock;
struct lock *addrmallock;
void *argmem;
void *addrmem;

/*
 * Filetable.
 */
 struct vnode;
 struct fileentry{ /*File entry struct*/
   off_t offset;
   int status_flag;
   int refcount;
   bool is_semfd;
   struct lock *lockfd;
   struct vnode *filevn;
 };
 
 struct ft{ /*Filetable struct*/
   struct fileentry *entry[__OPEN_MAX]; //max files per process : limits.h
 };

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Helper for fork(). You write this. */
void enter_forked_process(void *data1, unsigned long data2);

/* Enter user mode. Does not return. */
__DEAD void enter_new_process(int argc, userptr_t argv, userptr_t env,
		       vaddr_t stackptr, vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */
/*Initialize and free file table*/
int initialize_ft(void);
void free_ft(struct ft *ft);
int allocate_pid(struct proc *proc);

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);
int sys_close(int fd);
int sys_open(userptr_t filename, int flags, int mode, int *retval);
int sys_read(int fd, userptr_t buf, size_t buflen, int *retval);
int sys_write(int fd, userptr_t buf, size_t buflen, int *retval);
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_chdir(userptr_t pathname);
int sys___getcwd(userptr_t buf, size_t buflen, int *retval);

/*PROCESS SYSTEM CALLS (Asst 5)*/
int sys_fork(struct trapframe *tf, pid_t *retval);
pid_t sys_getpid(void);
int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval);
void sys__exit(int exitcode);
int sys_execv(const char *progname, char **args);

int getLen(char *s); //helper function for sys_execv
void fake_strcpy(char *dest, char *src, int len);
#endif /* _SYSCALL_H_ */
