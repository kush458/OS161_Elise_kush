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
 #include <kern/limits.h>
 #include <kern/unistd.h>
 #include <addrspace.h>
 #include <lib.h>
 #include <vfs.h>
 #include <thread.h>
 #include <copyinout.h>
 #include <uio.h>
 #include <vnode.h>
 #include <proc.h>
 #include <kern/syscall.h>
 #include <kern/seek.h>
 #include <kern/stat.h>
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
   KASSERT(ft1->entry[0] != NULL);
   
   ft1->entry[1] = kmalloc(sizeof(struct fileentry)); //stdout
   int res1;
   struct vnode *v1 = NULL;
   res1 = vfs_open(p1, O_WRONLY, 0664, &v1); //get vnode for stdout
   if(res1){;}
   ft1->entry[1]->filevn = v1;
   ft1->entry[1]->status_flag = O_WRONLY;
   ft1->entry[1]->offset = 0;
   KASSERT(ft1->entry[1] != NULL);
   
   ft1->entry[2] = kmalloc(sizeof(struct fileentry)); //stderr
   int res2;
   struct vnode *v2 = NULL;
   res2 = vfs_open(p2, O_WRONLY, 0664, &v2); //get vnode for stderr
   if(res2){;}
   ft1->entry[2]->filevn = v2;
   ft1->entry[2]->status_flag = O_WRONLY;
   ft1->entry[2]->offset = 0;
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
   for(int i = 0; i < __OPEN_MAX; i++){
     if(ft->entry[i] != NULL){
       vfs_close(ft->entry[i]->filevn);
       kfree(ft->entry[i]);
       ft->entry[i] = NULL;
     }
   }
   kfree(ft);
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
   p = copyinstr(filename, fn, __PATH_MAX, &size);
   if(p){  /*check filename*/
     kfree(fn);
     return p;  
   } 
   
   struct ft *ft = curproc->proc_ft; //Filetable of current process as declared in proc.h
   /*Find a free entry (An entry == NULL) in file table*/
   int fd;
   
   for(fd = 0; fd < __OPEN_MAX; fd++){
     if(ft->entry[fd] == NULL){
       //fd = i; //This the free entry
       break;
     }
   }
   if(fd == __OPEN_MAX){
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
   kfree(fn);
   *retval = fd;
   return 0;
 }
 /*
 FILE READ */
 int sys_read(int fd, userptr_t buf, size_t buflen, int *retval){
   struct ft *ftr = curproc->proc_ft; 
   /*Check fd*/
   if(fd < 0 || fd >= __OPEN_MAX){
     return EBADF; /*Bad File number*/
   }
   if(ftr->entry[fd] == NULL){
     return EBADF;
   }
   
   int flag = ftr->entry[fd]->status_flag & O_ACCMODE;
   if(flag == O_WRONLY){
     return EBADF;
   }
   


   /*As in uio.h(line 137) initialize a uio*/
   struct uio read_uio;
   struct iovec read_iov;
   int readoffset;
   readoffset = ftr->entry[fd]->offset;
   void *buf1 = kmalloc(4*buflen); /*Need to copyout this buf1*/
   uio_kinit(&read_iov, &read_uio, buf1, buflen, readoffset, UIO_READ);
   
   /*Call VOP_READ*/
   int ret = VOP_READ(ftr->entry[fd]->filevn, &read_uio);
   if(ret){
     return ret;
   }
   
   //void *buf1 = kmalloc(4*buflen);
   int p = copyout(buf1, buf, buflen);
   if(p){
   return p;
   }
  
   *retval = buflen - read_uio.uio_resid; 
   /*Increment the file offset by retval*/
   ftr->entry[fd]->offset = readoffset + *retval;
   return 0;
 }
 /*
 FILE WRITE */
 int sys_write(int fd, userptr_t buf, size_t buflen, int *retval){
   
   if((fd < 0) || (fd >= __OPEN_MAX)){
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
   int p = copyin(buf, buf1, buflen);
   if(p){
   return p;
   }
   
   /*As in uio.h(line 137) initialize a uio*/
   struct uio write_uio;
   struct iovec write_iov;
   int writeoffset;
   writeoffset = ftw->entry[fd]->offset;
   
   uio_kinit(&write_iov, &write_uio, buf1, buflen, writeoffset, UIO_WRITE);
   KASSERT(ftw->entry[fd]->filevn != NULL);
   
   /*Call VOP_WRITE*/
   int ret = VOP_WRITE(ftw->entry[fd]->filevn, &write_uio);
   if(ret){
     return ret;
   }

   *retval = buflen - write_uio.uio_resid; 
   /*Increment the file offset by retval*/
   ftw->entry[fd]->offset = write_uio.uio_offset;
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

   /*Don't necessarily need to use vfs_close()*/
   curft->entry[fd]->filevn = NULL;
   curft->entry[fd] = NULL; //Make it NULL to indicate entry is free
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
   if(ftl->entry[fd] == NULL || ftl->entry[fd]->filevn == NULL){
     return EBADF;
   }
   //vnode 
   if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END){
     return EINVAL; //See errno.h 
   }
   
   /*Now change the offset (ftl->entry[fd]->offset)*/
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
   if(newpos < 0){return EINVAL;} 
   
   /*Assign new offset to Current offset*/
   
   ftl->entry[fd]->offset = newpos;
   *retval = newpos;
   
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
   
   /*If newfd is open then close it as per man pages*/
   if(ftdup->entry[newfd] != NULL && ftdup->entry[newfd]->filevn != NULL){
     int ret = sys_close(newfd);
     if(ret){return ret;}
   }
   
   ftdup->entry[newfd] = ftdup->entry[oldfd]; 
   
   *retval = newfd;
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
 int sys___getcwd(userptr_t buf, size_t buflen, int *retval){
 
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
 
