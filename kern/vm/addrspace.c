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
#include <spl.h>
#include <mips/tlb.h>


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
struct ppages * dummyStart = NULL;
paddr_t firstaddr;
paddr_t lastaddr;

 void vm_bootstrap(void)
 {
	 lastaddr = ram_getsize();
         firstaddr = ram_getfirstfree();
	 KASSERT(firstaddr != 0);
	 KASSERT(lastaddr != 0);
	 mem_lock = (struct spinlock *)PADDR_TO_KVADDR(firstaddr);
	 spinlock_init(mem_lock);
	 long totalPages = (long)(lastaddr-firstaddr)/PAGE_SIZE;
	 KASSERT(sizeof(*pagetable[0]) == sizeof(*freelist));
	 //find the amount of pages needed to store the core map
	 long pagesNeeded = (totalPages*sizeof(*freelist)+(__PID_MAX/16)*sizeof(freelist))/PAGE_SIZE + 1;
	 KASSERT(totalPages-pagesNeeded > 0);
	 totalPages = totalPages-pagesNeeded;
	 //where the physical pages starts(after spinlock and coremap)
	 paddr_t pagestart = (firstaddr + pagesNeeded*PAGE_SIZE);

	 spinlock_acquire(mem_lock);
	 //waited until pagestart was set to increase firstaddr
	 firstaddr += sizeof(*mem_lock);
	 //initialize all memory as free
	 freelist = (struct ppages *)firstaddr;
         kprintf("%i", (int)totalPages);
	 struct ppages * vflist;
	 for(int i = 0; i < totalPages; i++){
            vflist = (struct ppages *)PADDR_TO_KVADDR((paddr_t)freelist);
            KASSERT(pagestart + i*PAGE_SIZE <= lastaddr-PAGE_SIZE);
            vflist->ppn = (paddr_t)(pagestart + i*PAGE_SIZE);
            vflist->numPages = 0;
            vflist->vpn = 0;
            vflist->pid = 1;
            KASSERT(firstaddr +(i+1)*sizeof(*freelist) < pagestart-sizeof(*freelist));
           if(i == totalPages-1){//last entry to coremap
              vflist->next = NULL;
           }
           else{
             vflist->next = (struct ppages *)(firstaddr + (i+1)*sizeof(*freelist));
             freelist = vflist->next;
           }
	 }
         struct ppages *vdStart;
         dummyStart = (struct ppages*)(freelist + totalPages*sizeof(*freelist));
         vdStart = (struct ppages *)PADDR_TO_KVADDR((paddr_t)dummyStart);
         vdStart->next = NULL;
         vdStart->numPages = 0;
         vdStart->ppn = 0;
         vdStart->vpn = -1;
         vdStart->pid = -1;
	 freelist = (struct ppages *)firstaddr;
	 //initialize empty alloclist dummy start, located where initialized freelist ends
         pagetable = (struct ppages **)(firstaddr + totalPages*sizeof(*freelist));
	 struct ppages **vpt = (struct ppages**)PADDR_TO_KVADDR((paddr_t)pagetable);

	 for(int i = 0; i < __PID_MAX/16; i++){
	   vpt[i] = dummyStart;
   }
	 spinlock_release(mem_lock);
	 return;

 }

 int vm_fault(int faulttype, vaddr_t faultaddress)
 {
//(void)faulttype;
//(void)faultaddress;
  paddr_t pad1;
  int entryhi, entrylo;
  struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR((paddr_t)pagetable);
  struct ppages *temp = pt[curproc->pid/16];

  if(faulttype == VM_FAULT_READ  || faulttype == VM_FAULT_WRITE){
      if(faultaddress >= USERSTACK){
        return EFAULT;
      }
      while(temp != NULL){
        if(temp->vpn == faultaddress){
          break;
        }
 
        temp = temp->next;
      }
  }
  pad1 = temp->ppn;
  int s = splhigh();
  entrylo = pad1 | TLBLO_VALID;
  entryhi = faultaddress;
  tlb_random(entryhi, entrylo);
  splx(s);
  
  return 0;


 }

 struct ppages * alloc_ppage(struct ppages *alloclist){
	 struct ppages * temp ;
   struct ppages * vfree;
   struct ppages * valloc;
   vfree = (struct ppages *)PADDR_TO_KVADDR((paddr_t)freelist);
   valloc = (struct ppages *)PADDR_TO_KVADDR((paddr_t)alloclist);
	 KASSERT(vfree->next != NULL); //no page eviction yet
   //move the front of free list to the front of the processes
   //alloclist and set the ppages fields
	 temp = vfree->next;
   if(valloc->ppn == 0){//if first entry in pagetableentry
      vfree->next = NULL;
   }
   else {
	  vfree->next = alloclist;
   }
   struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR((paddr_t)pagetable);
	 pt[curproc->pid/16] = freelist; //add new page to front of processes alloclist
   valloc = (struct ppages *)PADDR_TO_KVADDR((paddr_t)pt[curproc->pid/16]);
	 KASSERT(valloc->ppn == vfree->ppn);
	 KASSERT(valloc->ppn!=0);
	 valloc->vpn = (vaddr_t)PADDR_TO_KVADDR((paddr_t)valloc->ppn);
   valloc->pid = curproc->pid;
   valloc->numPages = 1;
	 freelist = temp;
	 return pt[curproc->pid/16];
 }

vaddr_t alloc_kpages(unsigned npages)
{
  if(freelist == NULL){//havent called vm_boostrap yet
    paddr_t pa = ram_stealmem(npages);
    KASSERT(pa != 0);
    return PADDR_TO_KVADDR(pa);//like dumbvm
  }
  else{
    spinlock_acquire(mem_lock);
    struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR((paddr_t)pagetable);
    struct ppages *alloclist = pt[curproc->pid/16];
   //grab a page
	  for(unsigned int i = 0; i < npages; i++){
	     alloclist = alloc_ppage(alloclist);
	  }
   //make sure the page returned is at the front of the alloc list
   KASSERT(pt[curproc->pid/16] == alloclist);
   struct ppages *valloc = (struct ppages *)PADDR_TO_KVADDR((paddr_t)alloclist);
   valloc->numPages = npages;
	 KASSERT(valloc->ppn != 0);
       vaddr_t out = PADDR_TO_KVADDR((paddr_t)valloc->ppn);
      KASSERT(out == valloc->vpn);
      spinlock_release(mem_lock);
	 return out;
 }
}


void free_kpages(vaddr_t addr)
{
  if(freelist == NULL){ //havent been through vm_boostrap
		return; //do nothing(like dumbvm, leak memory)
  }
  int front = 0;
  front = curproc->pid;
  spinlock_acquire(mem_lock);
  struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR((paddr_t)pagetable);
	struct ppages *alloclist = pt[curproc->pid/16];
  //get front of alloclist for this pid
	struct ppages *vcurr = (struct ppages *)PADDR_TO_KVADDR((paddr_t)alloclist);
  KASSERT(vcurr->ppn != 0); //this would mean the alloclist is empty

	struct ppages *prev = NULL;
	while(vcurr != NULL){
	  if((vaddr_t)vcurr->vpn == addr && curproc->pid == vcurr->pid){
	    KASSERT(vcurr->numPages != 0);
      //check to see if we have to update the front of the alloclist
      if(vcurr == (struct ppages *)PADDR_TO_KVADDR((paddr_t)alloclist)){
        front = 1;
      }
           int np = vcurr->numPages;
	    for(int i = 0; i < np; i++){
         if(front == 1){
            prev = freelist;
            freelist = pt[curproc->pid/16];
            if(vcurr->next == NULL){
              pt[curproc->pid/16] = dummyStart;
            }
	    else{
              pt[curproc->pid/16] = vcurr->next; 
            }
            vcurr->next = prev;
            vcurr->numPages = 0;
            vcurr->vpn = 0;
            vcurr->pid = 1;
            vcurr = pt[curproc->pid/16];
          }
          else{
            struct ppages *temp = prev->next;
            prev->next = vcurr->next;
            vcurr->next = freelist;
            freelist = temp;
            KASSERT(prev->next != vcurr && prev->next != vcurr->next);
            vcurr->numPages = 0;
            vcurr->vpn = 0;
            vcurr->pid = 1;
            vcurr = (struct ppages *)PADDR_TO_KVADDR((paddr_t)prev->next); //move on to next allocated mem
          }
	    }
      spinlock_release(mem_lock);
      return;
    }
   prev = vcurr;
   if(vcurr->next == NULL){
    vcurr = NULL;
   }
   else {
    vcurr = (struct ppages *)PADDR_TO_KVADDR((paddr_t)vcurr->next);
   }
  }
  spinlock_release(mem_lock);
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
        struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR(pagetable);
        as->pagetable = pt[curproc->pid/16];
	as->npages = 0;
	as->as_heaptop = 0;
	as->as_stackbottom = 0;
	as->as_stackpbase = 0;
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
        newas->npages = old->npages;
	newas->as_heaptop = old->as_heaptop;
	newas->as_stackbottom = old->as_stackbottom;
	newas->as_stackpbase = old->as_stackpbase;
	struct ppages *oldmem = (struct ppages *)PADDR_TO_KVADDR((paddr_t)old->pagetable);
	struct ppages *newmem = (struct ppages *)PADDR_TO_KVADDR((paddr_t)newas->pagetable);
	while(oldmem != NULL){
	   struct ppages * new = alloc_ppage(newas->pagetable);
	   newmem = (struct ppages *)PADDR_TO_KVADDR((paddr_t)new);
	   newmem->vpn = oldmem->vpn;
	   newmem->numPages = oldmem->numPages;
	   memmove((void *)PADDR_TO_KVADDR(newmem->ppn),(const void *)PADDR_TO_KVADDR(oldmem->ppn), PAGE_SIZE);
	   oldmem = (struct ppages *)PADDR_TO_KVADDR((paddr_t)oldmem->next);
	   struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR((paddr_t)pagetable);
	   newas->pagetable = pt[curproc->pid/16];
	   newmem = (struct ppages *)PADDR_TO_KVADDR((paddr_t)newas->pagetable);
	}

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	struct ppages **pt = (struct ppages **)PADDR_TO_KVADDR(pagetable);
	for(int i = 0; i < as->npages; i++){
	   struct ppages *currpage = (struct ppages *)PADDR_TO_KVADDR(as->pagetable);
	   free_kpages((vaddr_t)currpage);
	   as->pagetable = pt[curproc->pid/16];
	}
	kfree(as);
}

void
as_activate(void)
{
  int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);


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
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
	size_t npages = sz/PAGE_SIZE;
	(void)readable;
	(void)writeable;
	(void)executable;
	as->as_heaptop = as->as_heaptop + npages;
        return 0;
	
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * We don't do anything
	 */
	
	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Don't do anything
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Return stack pointer
	 */

	as->as_stackbottom = USERSTACK;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}
