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
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>


// #include <synch.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

// create a new address space including a pagetable(lv1, filled with 0) and regions track
struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) return NULL;

	/*
	 * Initialize as needed.
	 */

	as->regions = NULL;

	// allocate one frame as pt by looking up the frame table. this is the level one page table related to this process
	as->pt = (paddr_t **) alloc_kpages(1);
	if (as->pt == NULL) {
	    kfree(as);
	    return NULL;
	}

	// init the level one page table. bcz type is pointer, then one is 4 bytes(32 bits)
	for (int i=0; i<PTE_NUMBER; i++) {
		as->pt[i] = NULL;
	}

	// as->aslock = lock_create("aslock");
	// I got a double free error. at first i thought this due to threads and i try to 
	// add lock. but then i figure it out and find the reason is that, in vm_fault(line:77),
	// kfree() was used
	// TIPS: using: 'break kfree ;  condition X ptr == 0xwhatever' to debug double free error

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas == NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	if (old == NULL) {
		as_destroy(newas);
		return EINVAL;
	}

	struct region *oregion;
	struct region *nregion = newas->regions;

	// copy all the region in old to newas
	for (oregion = old->regions; oregion != NULL; oregion = oregion -> next){
		// create a new region for newas to hold the copy one
		struct region *tmp = kmalloc(sizeof(struct region));
		if (tmp == NULL){
			as_destroy(newas);
			return ENOMEM;
		}

		tmp->base = oregion->base;
		tmp->memsize = oregion->memsize;
		tmp->readable = oregion->readable;
		tmp->writeable = oregion->writeable;
		tmp->oldwriteable = oregion->oldwriteable;
		tmp->next = NULL;

		// check if this region is a head of the linked list
		if (nregion == NULL) {
			newas->regions = tmp;
		} else {
			nregion->next = tmp;
		}
		nregion = tmp;

		// copy the pagetable when level 2 pt exsit

		// create a level 2 page table at first
		for (int i = 0; i < PTE_NUMBER; i++){
			if (old->pt[i] != NULL){
				newas->pt[i] = (paddr_t *) alloc_kpages(1);
				if (newas->pt[i] == 0) {
					as_destroy(newas);
					return ENOMEM;
				}

				// copy entry and frame
				for (int j = 0; j < PTE_NUMBER; j++){
					if (old->pt[i][j] != 0){
						vaddr_t vpage = alloc_kpages(1);
       					if (vpage == 0) return ENOMEM;
        				// paddr_t pframe = kvaddr_to_paddr(vpage);
        				bzero((void *)vpage, PAGE_SIZE);
						// this function need vaddr. bcz we need to copy the whole page, no need to use offset
						memmove((void *) vpage, (const void *) paddr_to_kvaddr(old->pt[i][j] & PAGE_FRAME), PAGE_SIZE);
						// the add the frame number to the entry
						newas->pt[i][j] = (kvaddr_to_paddr(vpage) & PAGE_FRAME) | (TLBLO_DIRTY & old->pt[i][j]) | (TLBLO_VALID & old->pt[i][j]);
					} else {
						newas->pt[i][j] = 0;
					}
				}
			} 
			// else do nothing
		}

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

	if (as == NULL) return;

	// lock_acquire(as->aslock);

	// free the pagetable from level 2 to level 1
	for (int i = 0; i < PTE_NUMBER; i++){
		if (as->pt[i] != NULL){
			for (int j = 0; j < PTE_NUMBER; j++){
				if (as->pt[i][j] != 0){
					free_kpages(paddr_to_kvaddr(as->pt[i][j]) & PAGE_FRAME);
					// free this page in kseg0
				}
			}
			// after free all level 2 page table, we can free this entry of level one
			kfree(as->pt[i]);
		}
	}

	// after free all level one entry, we can free the whole page table
	kfree(as->pt);

	// and then we need to free the regions
	struct region *curr = as->regions;
	struct region *tmp;
	while(curr != NULL) {
		tmp = curr;
		curr = tmp->next;
		kfree(tmp);
	}
	// lock_release(as->aslock);
	// lock_destroy(as->aslock);
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

	// emmm... I just copy it from dumbvm

	int spl = splhigh();
	for (int i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
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
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	// check if the arguement is correct or not 
    if (as == NULL) return EFAULT;

	// for stack, it can queal to KSEG0
	if (vaddr + memsize > MIPS_KSEG0) return EFAULT;

	// maske sure the new region will not overlap other regions
	int flag = 0;
	struct region *curr = as->regions;
	while(curr != NULL) {
		if (curr->base >= vaddr && (curr->base <= vaddr + memsize)) {
			flag = 1;
		} else if ((vaddr+memsize) >= curr->base && (vaddr+memsize) <= (curr->base + curr->memsize)){
			flag = 1;
		} else if (vaddr >= curr->base && vaddr <= (curr->base + curr->memsize)){
			flag = 1;
		} else if ((vaddr+memsize) >= curr->base && (vaddr+memsize) <= (curr->base + curr->memsize)){
			flag = 1;
		}
		curr = curr->next;
	}
	if (flag == 1) return EFAULT;

    // This is also copied from dumbvm.c. It change all the memsize to 4096 ? 

    /* Align the region. First, the base... */
    memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    /* ...and now the length. */
    memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

    // then allocate a region
    struct region *reg = kmalloc(sizeof(struct region));
    if (reg == NULL) return ENOMEM;

    // then init the value in that region
    reg->base = vaddr;
    reg->memsize = memsize;
    reg->readable = readable;
    reg->writeable = writeable;
    reg->executable = executable;

    // regions are linked list in the addr space
    reg->next = as->regions;
    as->regions = reg;

	// return ENOSYS; /* Unimplemented */
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	if (as == NULL) return ENOMEM;

	struct region *curr = as->regions;
	while(curr != NULL){
		curr->oldwriteable = curr->writeable;
		curr->writeable = 1;
		curr = curr->next;
	}

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	for(int i = 0; i < NUM_TLB; i++) {
		if (as->pt[i] != NULL) {
			for(int j = 0; j < NUM_TLB; j++) {
				// not only the region may be changed, but the entry might be changed as well
				// this will lead the error of [f] case: not support read-only segments
				// by joining the index we can get the vaddr (page address)

				if (as->pt[i][j] != 0) {
					vaddr_t vaddr = i << 22|j << 12;
					// check the permission of this addr's region, if it is read only, we need to remove the writeable bit
					struct region *tregion = as->regions;
        			while (tregion != NULL) {
            			if (vaddr >= tregion->base && vaddr < (tregion->base + tregion->memsize)) break;
            			tregion = tregion->next;
        			}
					if (tregion == NULL) return EFAULT;

					if (tregion->oldwriteable == 0) {
						as->pt[i][j] = (as->pt[i][j] & PAGE_FRAME) | TLBLO_VALID;	// remove writeable
					}
				}
			}
		}
	}

    if (as == NULL) return ENOMEM;

	struct region *curr = as->regions;
	while(curr != NULL){
		curr->writeable = curr->oldwriteable;
		curr = curr->next;
	}

	as_activate();
	// flush TLB

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */
    if (as == NULL) return ENOMEM;

	/* Initial user-level stack pointer */
    *stackptr = USERSTACK;

    // since we only need treat the stack as other part, we just return as_define_region()
    return as_define_region(as, *stackptr - USRSTACKSIZE, USRSTACKSIZE, 1, 1, 1);


	return 0;
}


