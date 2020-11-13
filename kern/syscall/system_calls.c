/*
 *Assignment 4
 *
 */
 
 
 #include <types.h>
 #include <current.h>
 #include <syscall.h>
 #include <copyinout.h>
 #include <kern/fcntl.h>
 #include <kern/errno.h>
 #include <mips/trapframe.h>
 #include <kern/limits.h>
 #include <limits.h>
 #include <kern/unistd.h>
 #include <addrspace.h>
 #include <lib.h>
 #include <vfs.h>
 #include <thread.h>
 #include <vm.h>
 #include <copyinout.h>
 #include <uio.h>
 #include <vnode.h>
 #include <proc.h>
 #include <synch.h>
 #include <kern/syscall.h>
 #include <kern/seek.h>
 #include <kern/stat.h>
 #include <kern/wait.h>
 int initialize_ft(void){  /*Initialize Filetable*/
   
   /*Initialize ft: fd, 0,1,2 are for stdin, stdout and stderr respectively */
   struct ft *ft1 = kmalloc(sizeof(struct ft));
   //from piazza @737 (helpful for inializing) 
   /*initializing ft for stdin(0), stdout(1), stderr(2)*/
   
   ft1->entry[0] = kmalloc(sizeof(struct fileentry)); //stdin
   struct vnode *v = NULL;
   char *p = kstrdup("con:");
   char *p1 = kstrdup("con:");
   char *p2 = kstrdup("con:");
   int res0;
   res0 = vfs_open(p, O_RDONLY, 0664, &v); //get vnode for stdin
   if(res0){;}
   ft1->entry[0]->filevn = v;
   ft1->entry[0]->status_flag = O_RDWR;
   ft1->entry[0]->offset = 0;
   ft1->entry[0]->refcount = 1;
   ft1->entry[0]->is_semfd = false;
   ft1->entry[0]->lockfd = lock_create("stdin");
   KASSERT(ft1->entry[0] != NULL);
   
   ft1->entry[1] = kmalloc(sizeof(struct fileentry)); //stdout
   int res1;
   struct vnode *v1 = NULL;
   res1 = vfs_open(p1, O_WRONLY, 0664, &v1); //get vnode for stdout
   if(res1){;}
   ft1->entry[1]->filevn = v1;
   ft1->entry[1]->status_flag = O_WRONLY;
   ft1->entry[1]->offset = 0;
   ft1->entry[1]->refcount = 1;
   ft1->entry[1]->is_semfd = false;
   ft1->entry[1]->lockfd = lock_create("stdout");   
   KASSERT(ft1->entry[1] != NULL);
   
   ft1->entry[2] = kmalloc(sizeof(struct fileentry)); //stderr
   int res2;
   struct vnode *v2 = NULL;
   res2 = vfs_open(p2, O_WRONLY, 0664, &v2); //get vnode for stderr
   if(res2){;}
   ft1->entry[2]->filevn = v2;
   ft1->entry[2]->status_flag = O_WRONLY;
   ft1->entry[2]->offset = 0;
   ft1->entry[2]->refcount = 1;
   ft1->entry[2]->is_semfd = false;
   ft1->entry[2]->lockfd = lock_create("stderr");
   KASSERT(ft1->entry[2] != NULL);
   
   for(int i = 3; i < __OPEN_MAX; i++){
     ft1->entry[i] = NULL; /*Default value for all unopened files*/
   }
   
   curproc->proc_ft = ft1; //filetable of the current process, modified proc.h()
   
   KASSERT(curproc->proc_ft != NULL);
   
   kfree(p);
   kfree(p1);
   kfree(p2);
   return 0;
 }
 
 
 /*Free Filetable (Might be used later on)*/
 void free_ft(struct ft *ft){
   (void)ft;
   //for(int i = 0; i < __OPEN_MAX; i++){
   //  if(ft->entry[i] != NULL){
   //    vfs_close(ft->entry[i]->filevn);
   //    lock_destroy(ft->entry[i]->lockfd);
   //    kfree(ft->entry[i]);
   //    ft->entry[i] = NULL;
   //  }
  // }
   //kfree(ft);
 }
 
 /*
  * FILE OPEN
  */
 int sys_open(userptr_t filename, int flags, int mode, int *retval){
   
   //vfspath.c is helpful

   /*copyinstr*/                            //NAME_MAX
   char *fn = (char *)kmalloc(sizeof(char) * 255);
   int p;      
   size_t size =0;
   p = copyinstr(filename, fn, PATH_MAX, &size);
   if(p){  /*check filename*/
     kfree(fn);
     return p;  
   } 
   /*Check if the file is a sem file*/
   char substr[5];
   memcpy(substr, &filename[0], 4);
   char *str1 = kstrdup("sem:\0");

   struct ft *ft = curproc->proc_ft; //Filetable of current process as declared in proc.h
   /*Find a free entry (An entry == NULL) in file table*/
   int fd;
   
   for(fd = 0; fd < OPEN_MAX; fd++){
     if(ft->entry[fd] == NULL){
       //fd = i; //This the free entry
       break;
     }
   }
   if(fd == OPEN_MAX){
     return EMFILE;
   }
   
   ft->entry[fd] = kmalloc(sizeof(struct fileentry));
   struct vnode *vn = NULL;
   int ret;
   ret = vfs_open(fn, flags, mode, &vn); /*Get file vnode*/
   if(ret){
     return ret;
   }
   ft->entry[fd]->filevn = vn;
   ft->entry[fd]->offset = 0;
   ft->entry[fd]->status_flag = flags;
   ft->entry[fd]->refcount = 1;
   if(strcmp(str1, substr)==0){ /*Check if the file is a semfile*/
     ft->entry[fd]->is_semfd = true;
   }else{
     ft->entry[fd]->is_semfd = false;  
   }
   ft->entry[fd]->lockfd = lock_create("lockfd");
   kfree(fn);
   *retval = fd;
   return 0;
 }
 /*
 FILE READ */
 int sys_read(int fd, userptr_t buf, size_t buflen, int *retval){
   struct ft *ftr = curproc->proc_ft; 
   /*Check fd*/
   if(fd < 0 || fd >= OPEN_MAX){
     return EBADF; /*Bad File number*/
   }
   if(ftr->entry[fd] == NULL){
     return EBADF;
   }
   
   int flag = ftr->entry[fd]->status_flag & O_ACCMODE;
   if(flag == O_WRONLY){
     return EBADF;
   }
   
   lock_acquire(ftr->entry[fd]->lockfd); /*lock acquire*/

   /*As in uio.h(line 137) initialize a uio*/
   struct uio read_uio;
   struct iovec read_iov;
   int readoffset;
   readoffset = ftr->entry[fd]->offset;
   void *buf1 = kmalloc(4*buflen); /*Need to copyout this buf1*/
   if(ftr->entry[fd]->is_semfd){
     uio_kinit(&read_iov, &read_uio, buf, buflen, readoffset, UIO_READ);
   }
   else{
     uio_kinit(&read_iov, &read_uio, buf1, buflen, readoffset, UIO_READ);   
   }
   /*Call VOP_READ*/
   int ret = VOP_READ(ftr->entry[fd]->filevn, &read_uio);
   if(ret){
     lock_release(ftr->entry[fd]->lockfd); /*lock release*/
     return ret;
   }
   
   //void *buf1 = kmalloc(4*buflen);
   if(ftr->entry[fd]->is_semfd == false){
     int p = copyout(buf1, buf, buflen);
     if(p){
       kfree(buf1);
       lock_release(ftr->entry[fd]->lockfd); /*lock release*/
       return p;
     } 
   }
  
   *retval = buflen - read_uio.uio_resid; 
   /*Increment the file offset by retval*/
   ftr->entry[fd]->offset = read_uio.uio_offset;//readoffset + *retval;
   lock_release(ftr->entry[fd]->lockfd); /*lock release*/
   kfree(buf1);
   return 0;
 }
 /*
 FILE WRITE */
 int sys_write(int fd, userptr_t buf, size_t buflen, int *retval){
   
   if((fd < 0) || (fd >= OPEN_MAX)){
     return EBADF; /*Bad File number*/
   }
   
   struct ft *ftw = curproc->proc_ft;
   if(curproc->proc_ft == NULL){
     *retval = -1;
     return EBADF;
   }
   
   if(ftw->entry[fd] == NULL || ftw->entry[fd]->filevn == NULL ){
     return EBADF;
   }
   
   
   if(!(ftw->entry[fd]->status_flag & O_ACCMODE)){
     return EBADF;
   }
   //copyin
   void *buf1 = kmalloc(4*buflen);
   if(ftw->entry[fd]->is_semfd == false){
     int p = copyin(buf, buf1, buflen);
     if(p){
       kfree(buf1);
       return p;
     } 
   }
   lock_acquire(ftw->entry[fd]->lockfd); /*lock acquire*/
   /*As in uio.h(line 137) initialize a uio*/
   struct uio write_uio;
   struct iovec write_iov;
   int writeoffset;
   writeoffset = ftw->entry[fd]->offset;
   if(ftw->entry[fd]->is_semfd){
     uio_kinit(&write_iov, &write_uio, buf, buflen, writeoffset, UIO_WRITE);
   }
   else{
     uio_kinit(&write_iov, &write_uio, buf1, buflen, writeoffset, UIO_WRITE);
   }
   KASSERT(ftw->entry[fd]->filevn != NULL);
    
   /*Call VOP_WRITE*/
   int ret = VOP_WRITE(ftw->entry[fd]->filevn, &write_uio);
   kfree(buf1);
   if(ret){
     lock_release(ftw->entry[fd]->lockfd); /*lock release*/
     return ret;
   }
  
   *retval = buflen - write_uio.uio_resid; 
   /*Increment the file offset by retval*/
   ftw->entry[fd]->offset = write_uio.uio_offset;
   lock_release(ftw->entry[fd]->lockfd); /*lock release*/
   return 0;
 }
 /*FILE CLOSE*/
 int sys_close(int fd){
  
   /*if(curproc->proc_ft == NULL || curproc->proc_ft->entry[fd] == NULL){
     return EBADF;
   }*/
   struct ft *curft = curproc->proc_ft;
   /*Check fd*/
   if(fd < 0 || fd >= __OPEN_MAX){
     return EBADF; /*Bad File number*/
   }
   
   if(curft->entry[fd] == NULL){
     return EBADF;
   }
   lock_acquire(curft->entry[fd]->lockfd); /*lock acquire*/
   curft->entry[fd]->refcount -= 1;
   /*Don't necessarily need to use vfs_close()*/
   if(curft->entry[fd]->refcount == 0){
     lock_release(curft->entry[fd]->lockfd); /*lock release*/
     lock_destroy(curft->entry[fd]->lockfd); /*lock destroy*/
     curft->entry[fd]->filevn = NULL;
     curft->entry[fd] = NULL; //Make it NULL to indicate entry is free
     return 0;
   }
   lock_release(curft->entry[fd]->lockfd); /*lock release*/
   return 0;
 }
 /*
  * LSEEK
  */
 int sys_lseek(int fd, off_t pos, int whence, off_t *retval){
  /*
  *If whence is
  * SEEK_SET, the new position is pos.
  * SEEK_CUR, the new position is the current position plus pos.
  * SEEK_END, the new position is the position of end-of-file plus pos.
  */
  
  struct ft *ftl = curproc->proc_ft;
  
   /*Check fd*/
   if(fd < 0 || fd >= __OPEN_MAX){
     return EBADF; /*Bad File number*/
   }
   if(ftl->entry[fd] == NULL || ftl->entry[fd]->filevn == NULL || ftl->entry[fd]->refcount == 0){
     return EBADF;
   }
   //vnode 
   if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END){
     return EINVAL; //See errno.h 
   }
   if(!VOP_ISSEEKABLE(ftl->entry[fd]->filevn)){ /*Check lseek on device*/
     return ESPIPE;
   }
   /*Now change the offset (ftl->entry[fd]->offset)*/
   lock_acquire(ftl->entry[fd]->lockfd); /*lock acquire*/
   off_t curpos = ftl->entry[fd]->offset;
   off_t newpos;
   if(whence == SEEK_SET){
      newpos = pos;
   }
   else if(whence == SEEK_CUR){
      newpos = pos + curpos;
   }
   else if(whence == SEEK_END){
      /* kern/stat.h has the file size (line 44)*/
      struct stat file_stat;
      VOP_STAT(ftl->entry[fd]->filevn, &file_stat);
      newpos = file_stat.st_size + pos;
   }
   if(newpos < 0){
     lock_release(ftl->entry[fd]->lockfd); /*lock release*/
     return EINVAL;
     } 
   
   /*Assign new offset to Current offset*/
   
   ftl->entry[fd]->offset = newpos;
   *retval = newpos;
   lock_release(ftl->entry[fd]->lockfd); /*lock release*/
   return 0;
 }
 /*
  * DUP2
  */
 int sys_dup2(int oldfd, int newfd, int *retval){
 
   struct ft *ftdup = curproc->proc_ft;
   /*Check if both fds are same*/
   if(oldfd == newfd){
     *retval = newfd;
     return 0;
   }
   
   /*Check if both fds are valid*/
   if(oldfd < 0 || oldfd >= __OPEN_MAX || newfd < 0 || newfd >= __OPEN_MAX ){
     return EBADF; /*Bad File number*/
   }
   
   if(ftdup->entry[oldfd] == NULL){
     return EBADF;
   }
   lock_acquire(ftdup->entry[oldfd]->lockfd); /*lock acquire*/
   /*If newfd is open then close it as per man pages*/
   if(ftdup->entry[newfd] != NULL && ftdup->entry[newfd]->filevn != NULL){
     int ret = sys_close(newfd);
     if(ret){return ret;}
   }
   ftdup->entry[oldfd]->refcount++;
   ftdup->entry[newfd] = ftdup->entry[oldfd]; 
   
   *retval = newfd;
   lock_release(ftdup->entry[oldfd]->lockfd); /*lock release*/
   return 0;
 }
 /*
  * CHDIR
  */
 int sys_chdir(userptr_t pathname){
   
   char *path = (char *)kmalloc(__PATH_MAX);
   int ret;
                   //usersrc,dest
   ret = copyinstr(pathname, path, __PATH_MAX, NULL);
   
   if(ret){
     kfree(path);
     return ret;
   }
   
   vfs_chdir(path);
   kfree(path);
   return 0;
 
 }
 /*
  * GETCWD
  */
 int sys___getcwd(userptr_t buf, size_t buflen, pid_t *retval){
 
   /*Need to set up a uio and call vfs_getcwd()*/

   struct uio cwd_uio;
   struct iovec cwd_iov;
   void *buf1 = kmalloc(4*buflen); /*Need to copyout this buf1*/
   uio_kinit(&cwd_iov, &cwd_uio, buf1, buflen, 0, UIO_READ);
   
   vfs_getcwd(&cwd_uio);
   //void *buf1 = kmalloc(4*buflen);
   int p = copyout(buf1, buf, buflen);
   if(p){
   return p;
   }
   
   *retval = buflen - cwd_uio.uio_resid;
   
   return 0;
   
 }
 /*
  * ALLOCATE PID
  */
 int allocate_pid(struct proc *proc){
   /*Allocate a pid to a non-null process*/
   int i;
   for(i = PID_MIN; i < PID_MAX; i++){
      if(proc_ids[i] == NULL){
        proc_ids[i] = proc;
        break;
      }
    
   }
   if(i == PID_MAX){
     return EMPROC;
   }
    
   return i;
 }
 /*
  * FORK system call
  */ 
 int sys_fork(struct trapframe *tf, int *retval){
    /* Things we need to copy (see lecture slides):
     *   copy addr space
     *   copy filetable
     *   copy and tweak trapframe (arch state)
     *   copy kernel thread (thread_fork()) (need a child process too) 
     *   Return to usermode (mips_usermode(), enter_forked_process())
     */
    
    struct addrspace *caddr = NULL;
    struct trapframe *ctrapframe = NULL;
    
    /*Copy addr space*/
    int p;     //src                , ret
    p = as_copy(curproc->p_addrspace, &caddr);
    if(caddr == NULL){
      return ENOMEM;
    }
    
    /*Copy filetable (from parent to child process))*/
    struct proc *cproc = proc_return("Childprocess");
    if(cproc == NULL){
      return ENOMEM;
    }
    
    cproc->proc_ft = curproc->proc_ft;
    for(int i = 0; i < OPEN_MAX; i++){
      if(cproc->proc_ft->entry[i] != NULL){
        cproc->proc_ft->entry[i]->refcount++;
      }
    } 
    cproc->ppid = curproc->pid;
        
   /*copy trapframe*/
   cproc->p_addrspace = caddr; //copy child addrspace to childporc's addrspace
   //need to activate the addrspace in enter_forked_process
   ctrapframe = kmalloc(sizeof(struct trapframe));
   memcpy(ctrapframe, tf, (sizeof(struct trapframe)));
   
   /*Copy kernel thread
   for reference: int thread_fork(const char *name, struct proc *proc,
                void (*func)(void *, unsigned long),
                void *data1, unsigned long data2);*/

   p = thread_fork("Childthread", cproc, enter_forked_process, (struct trapframe*)ctrapframe, (unsigned long)NULL);     
   if(p){
     return p;
   }
   /*cproc's cwd (see proc_create_runprogram)*/
   if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		cproc->p_cwd = curproc->p_cwd;
   }
   /*Need to return child pid so, *retval = childpid*/
   *retval = cproc->pid;

   return 0;
 }
 /*
  * GETPID system call
  */
 pid_t sys_getpid(void){
   return curproc->pid;
 }
 /*
  * EXIT system call
  */
 void sys__exit(int exitcode){
   //prof said in lecture to use cvs
   /*If a parent process exits before 
   one or more of its children, it can no 
   longer be expected collect their exit status.*/
   //proc_ids[curproc->ppid] is the parent
   KASSERT(proc_ids[curproc->pid] != NULL); 
   lock_acquire(curproc->proclock); /*lock acquire*/
   if(exitcode){
     if(exitcode == 107){ /*MAGIC_STATUS*/
       curproc->ecode = _MKWAIT_EXIT(exitcode);
      }else{
       curproc->ecode = _MKWAIT_SIG(exitcode);}
   }else{
      curproc->ecode = _MKWAIT_EXIT(exitcode);
   }
   curproc->exitflag = true;
   cv_signal(curproc->exitcv, curproc->proclock);
   lock_release(curproc->proclock); /*lock release*/
   thread_exit();
   
 } 
 /*
  * WAITPID system call
  */
 int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval){
   (void)options;
   /*The pid argument named a non-existent process*/
   if(proc_ids[pid] == NULL || (pid < PID_MIN) || (pid > OPEN_MAX)){
     return ESRCH;
   }   
   /*return ECHILD if pid argument named a process that was not a child of the current process*/
   if((proc_ids[pid]->ppid != curproc->pid) && (curproc->pid != 2)){ 
     return ECHILD;
   }
   
   /*return EFAULT if status is an invalid pointer (from vm.h)*/
   if(status == (int *)USERSPACETOP || status == (int *)0x40000000 || ((int)status & 3) != 0){
     return EFAULT;
   } 
   
   if(options != 0){
   return EINVAL;
   }
   lock_acquire(proc_ids[pid]->proclock);/*lock acquire*/
   
   if(proc_ids[pid]->exitflag == false){ //check if child has exited
     
     cv_wait(proc_ids[pid]->exitcv, proc_ids[pid]->proclock);    
    

   }
   if(status != NULL){
   *status = proc_ids[pid]->ecode; }
   
   lock_release(proc_ids[pid]->proclock);/*lock release*/
   
   lock_destroy(proc_ids[pid]->proclock);
   cv_destroy(proc_ids[pid]->exitcv);
   proc_destroy(proc_ids[pid]);
   proc_ids[pid] = NULL;
   
   *retval = pid;
   return 0;
 }


/*
* EXECV system call
*/
int
sys_execv(const char *progname, char **args)
{
   struct addrspace *as;
   struct addrspace *oldas;
   struct vnode *v;
   vaddr_t entrypoint, stackptr;
   userptr_t usrsp;
   int result = 0;
   int numArgs = 0;
   char *kbuf;
   kbuf = kmalloc(__PATH_MAX);
   char *argString;
   size_t stringLength = 0;
   size_t totalLength = 0;
   char **bigKargs;
   memset(kbuf, '\0', sizeof(kbuf));
   /*return EFAULT if either param is NULL)*/
   if(args == NULL || progname == NULL){
      return EFAULT;
   }	

   /*return EFAULT if progname or args not a valid pointer*/
   if(args == (char **)USERSPACETOP || args == (char **)0x40000000 || ((int)args & 3) != 0){
       return EFAULT;
   }

   result = copyin((userptr_t)progname, kbuf, PATH_MAX);
   if(result){
      return result;
   }
	
   /*Open the file*/
   result = vfs_open(kbuf, O_RDONLY, 0, &v);
   if (result) {
      return result;
   }

   /*Copy in the arguments to kernel space*/
   //args is a ptr to a null terminated array
   //find argument count
   while(args[numArgs] != NULL && numArgs < (ARG_MAX)){
      numArgs++;
   }
   if(numArgs == ARG_MAX){
      return E2BIG;	
   }

   argString = argmalloc();	
   bigKargs = addrmalloc();
   int offset = 0;
   //go through args, malloc space for each string, and store that
   //address in the arg array
   /*Initialize the array of pointers to NULL*/
   for(int i = 0; i < numArgs; i++){
       bigKargs[i] = NULL;
   }
   for (int i = 0; i < numArgs; i++){
       if(totalLength < ARG_MAX){
           result = copyinstr((userptr_t)args[i], kbuf, __PATH_MAX, &stringLength);
           if(result){
             result = copyinstr((userptr_t)args[i], &argString[offset], __ARG_MAX, &stringLength);
             if(result){
               argfree(argString);
               addrfree((void **)bigKargs);
               return result;
             }
             if((signed int)ARG_MAX - (signed int)totalLength - (signed int)stringLength < 0){
                 argfree((void *)argString);
                 addrfree((void **)bigKargs);
                 return E2BIG;
              }
	     }
             else {
                memcpy(&argString[offset], kbuf, stringLength);
              }
              bigKargs[i] = &argString[offset];
              offset += stringLength;
              totalLength += stringLength;	
        }
        else{ //if go over ARG_MAX
	    argfree((void *)argString);
             addrfree((void **)bigKargs);
             return E2BIG;
        }
   }

   kfree(kbuf);

   /* Create a new address space. */
   as = as_create();
   if (as == NULL) {
       vfs_close(v);
       argfree((void *)argString);
        addrfree((void **)bigKargs);
        return ENOMEM;
    }

    /* Switch to it and activate it. */
    /* Save the old address space */
    oldas = proc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) { //return to old address space
        proc_setas(oldas);
        as_destroy(as); //destroy the new address space
         argfree((void *)argString);
         addrfree((void **)bigKargs);
         vfs_close(v);
         return result;
    }

    /* Done with the file now. */
    vfs_close(v);


    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
         proc_setas(oldas);
         as_destroy(as);
         argfree((void *)argString);
         addrfree((void **)bigKargs);
         return result;
    }

    /*Copy args into new address space, correctly arranged*/
    usrsp = (userptr_t)stackptr;
    for(int i = 0; i < numArgs; i++){   
       stringLength = getLen(bigKargs[i]);
       //decrease sp by size of string at kArgs[i] + null needed to align string to 4
       usrsp = (userptr_t)((int)usrsp - stringLength - (4-stringLength%4));
       KASSERT((int)usrsp%4 == 0);
       result = copyoutstr(bigKargs[i], usrsp, stringLength, NULL);
       if(result) {
         argfree((void *)argString);
         addrfree((void**)bigKargs);
         return result;
       }
       //kfree(bigKargs[i]);
       bigKargs[i] = (char*)usrsp; //save each new user address of string 
     }
     bigKargs[numArgs] = NULL;
     //push the null-terminated array of the locations of the 
     //strings in userspace
     usrsp =(userptr_t)((int)usrsp-(4*(numArgs+1)));
     result = copyout(bigKargs, usrsp, 4*(numArgs+1));

     if (result) {
        proc_setas(oldas);
        as_destroy(as);
        argfree((void *)argString);
        addrfree((void **)bigKargs);
        return result;
     }

     /* Clean up old address space and kernel heap*/
     as_destroy(oldas);
     argfree((void *)argString);
     addrfree((void **)bigKargs);
     stackptr = (vaddr_t)usrsp;
     /* Warp to user mode. */
     enter_new_process(numArgs, usrsp /*userspace addr of argv*/,
                          NULL /*userspace addr of environment*/,
                          (vaddr_t)usrsp, entrypoint);

     /* enter_new_process does not return. */
     panic("enter_new_process returned\n");
     return result;
}

                                      
int getLen(char *s){
  	int i = 0;
	int len = 0;
	while(s[i] != '\0'){
	  len++;
	  i++;
	}
	return len + 1; //include the null terminator
}
