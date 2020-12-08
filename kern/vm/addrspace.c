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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <spinlock.h>
#include <current.h>
#include <proc.h>

//wrap the free and alloced mem structures in a spinlock
//possibly slow if search time takes long(initially when no alloced mem)
struct spinlock *mem_lock;

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */
struct ppages *freelist = NULL;
struct ppages ** pagetable = NULL;

 void vm_bootstrap(void)
 {
	kprintf("hello");
         spinlock_init(mem_lock);
	 paddr_t firstaddr = ram_getfirstfree();
	 paddr_t lastaddr = ram_getsize();
	 KASSERT(firstaddr != 0);
	 KASSERT(lastaddr != 0);
	 unsigned long totalPages = (unsigned long)(lastaddr-firstaddr)/PAGE_SIZE;
	 KASSERT(sizeof(*pagetable[0]) == sizeof(*freelist));
	 //find the amount of pages needed to store the core map
         float pages = (float)((totalPages+1)*sizeof(*freelist))/(float)PAGE_SIZE;
	 unsigned long pagesNeeded = (unsigned long)pages;
	 if(pages - (float)pagesNeeded >= .5){
		 pagesNeeded += 1;
	 }
	 KASSERT(totalPages-pagesNeeded > 0);
	 totalPages = totalPages-pagesNeeded;
	 //store the coremap at the first available free address
	 freelist = (struct ppages *)firstaddr;
	 //where the physical pages start(after coremap)
	 paddr_t pagestart = (firstaddr + pagesNeeded*PAGE_SIZE);
	 spinlock_acquire(mem_lock);
	 //initialize all memory as free
         kprintf("%i", (int)totalPages); 
	 for(unsigned int i = 0; i < totalPages; i++){
		  KASSERT(pagestart + i*PAGE_SIZE < lastaddr- PAGE_SIZE);
	    freelist->ppn = pagestart + i*PAGE_SIZE;
			freelist->numPages = 0;
			freelist->vpn = -1;
			//freelist->pid = NULL;
			KASSERT(firstaddr +(i+1)*sizeof(*freelist) < pagestart-sizeof(*freelist));
			if(i == totalPages-1){//last entry to coremap
				freelist->next = NULL;
			}
			else{
			  freelist->next = (struct ppages *)(firstaddr + (i+1)*sizeof(freelist));
			  freelist = freelist->next;
		  }
	 }
	 //initialize empty alloclist dummy start, located where initialized freelist ends
	 for(int i = 0; i < __PID_MAX; i++){
	   pagetable[i] = (struct ppages *)(firstaddr + totalPages*sizeof(*freelist) + i*sizeof(*freelist));
	   pagetable[i]->next = NULL;
	   pagetable[i]->ppn = 0;
	   pagetable[i]->vpn = -1;
	   //pagetable[i]->pid = NULL;
	   pagetable[i]->numPages = 0;
   }
	 spinlock_release(mem_lock);
	 return;

 }

 int vm_fault(int faulttype, vaddr_t faultaddress)
 {
(void)faulttype;
(void)faultaddress;
return 0;

 }

 struct ppages * alloc_ppage(struct ppages *alloclist){
	struct ppages * temp = NULL;
	 KASSERT(freelist->next != NULL); //no page eviction
	 temp = freelist->next;
	 freelist->next = alloclist;
	 alloclist = freelist;
	 KASSERT(alloclist->ppn == freelist->ppn);
	 KASSERT(alloclist->ppn!=0);
	 alloclist->vpn = PADDR_TO_KVADDR(alloclist->ppn);
	 //alloclist->pid = curproc;
         alloclist->numPages = 1;
	 freelist = temp;
         pagetable[curproc->pid] = alloclist;
	 return alloclist;
 }

vaddr_t alloc_kpages(unsigned npages)
{
 struct ppages *alloclist = pagetable[curproc->pid];
 if(freelist == NULL || alloclist == NULL){//havent called vm_boostrap yet
	 return ram_stealmem(npages);//like dumbvm
 }
 else{
	 struct ppages *temp = NULL;
	 if(alloclist->ppn == 0){//if this is first allocation
		 alloclist = freelist;
		 KASSERT(alloclist->ppn == freelist->ppn);
		 KASSERT(freelist->next != NULL); //no page eviction
		 KASSERT(alloclist->ppn != 0);
		 alloclist->vpn = PADDR_TO_KVADDR(alloclist->ppn);
		 //alloclist->pid = curproc;
		 freelist = freelist->next;
		 KASSERT(alloclist != freelist);
		 alloclist->next = NULL;
                 pagetable[curproc->pid] = alloclist;
		 if(npages == 1){
			 alloclist->numPages = 1;
		 }
		 else {
		   alloclist->numPages = 0;
	   }
	 }
	 else {
		alloclist = alloc_ppage(alloclist);
		if(npages == 1){
 		  alloclist->numPages = npages;
 	  }
 	  else{
 		  alloclist->numPages = 0;
 	  }
	 }
	 if(npages != 1){
	   for(unsigned int i = 1; i < npages; i++){
	      KASSERT(freelist->next != NULL);//no page eviction
	      temp = freelist->next;
	      freelist->next = alloclist;
	      alloclist = freelist;
	      KASSERT(alloclist->ppn == freelist->ppn);
	      KASSERT(alloclist->ppn!=0);
	      alloclist->vpn = PADDR_TO_KVADDR(alloclist->ppn);
	      //alloclist->pid = curproc;
	      freelist = temp;
              pagetable[curproc->pid] = alloclist;
	      if(i == npages-1){
		 alloclist->numPages = npages;
	      }
	      else{
                 alloclist->numPages = 0;
	      }
	   }
	 }
         KASSERT(pagetable[curproc->pid] == alloclist);
	 KASSERT(alloclist->ppn != 0);
	 return PADDR_TO_KVADDR(alloclist->ppn);
 }
}


void free_kpages(vaddr_t addr)
{
  struct ppages *alloclist = pagetable[curproc->pid];
  int front = 0;
  if(freelist == NULL || alloclist == NULL){ //havent been through vm_boostrap
		return; //do nothing(like dumbvm, leak memory)
  }
	struct ppages *curr = alloclist;
	struct ppages *prev = NULL;
	while(curr->next != NULL){
	  if(curr->vpn == addr){
	    KASSERT(curr->numPages != 0);
            if(curr == alloclist){
            front = 1;
            }
	    for(int i = 0; i < curr->numPages; i++){
              if(front == 1){
                pagetable[curproc->pid] = curr->next;
                curr->next = freelist;
                freelist = curr;
                freelist->numPages = 0;
                freelist->vpn = -1;
                curr = pagetable[curproc->pid];
                //freelist->pid = NULL;
              }
              else
              {
                prev->next = curr->next;
	        curr->next = freelist;
	        freelist = curr;
	        freelist->numPages = 0;
	        freelist->vpn = -1;
	        //freelist->pid = NULL;
	         curr = prev->next;
             }
	   }
	return;
        }
      curr = curr->next;
      }
  kprintf("Not in processes alloclist");
  return;
}

void vm_tlbshootdown_all(void)
{
 return;
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
  return;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}
