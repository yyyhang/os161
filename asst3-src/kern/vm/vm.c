#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>
#include <current.h>
#include <elf.h>
#include <spl.h>

/* Place your page table functions here */

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

/* Design of vm_fault:
 *      1. check the fault type and if the faultaddress is valid
 *      2. check if the entry in the pagetable
 *      3. add to TLB
 */


int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    switch (faulttype){
        case VM_FAULT_READONLY:
        return EFAULT;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
    }

    if (curproc == NULL) return EFAULT;

    // get the addr space of current process
    struct addrspace *as = proc_getas();
    if (as == NULL) return EFAULT;

    if (faultaddress == 0) return EFAULT;
    
    // change the vaddr to paddr, and take out the index ? 
    // but i don't think we need to use paddr to get page entry, actually, we need vaddr bits to get the entry
    // paddr_t paddr = kvaddr_to_paddr(faultaddress);

    uint32_t fbits = faultaddress >> 22;
    uint32_t mbits = (faultaddress << 10) >> 22;

    // if it is not in the page table, we need to add entry to page table

    // first case: no the whole level 2 pagetable

    if (as->pt[fbits] == NULL){
        as->pt[fbits] = (paddr_t *) alloc_kpages(1);
        if (as->pt[fbits] == 0) return ENOMEM;
        for (int i = 0; i < PTE_NUMBER; i++){
            as->pt[fbits][i] = 0;
        }
    }
    
    // second case: no entry in the page table. if meet case 1, must meet case 2

    if (as->pt[fbits][mbits] == 0){
        // first, check if the addr is valid or not, and get what region it is
        struct region *tregion = as->regions;
        while (tregion != NULL) {
            if (faultaddress >= tregion->base && faultaddress < (tregion->base + tregion->memsize)){
                break;
            }
            tregion = tregion->next;
        }
        
        if (tregion == NULL){
            // kfree(as->pt[fbits]);
            return EFAULT;
        }

        // allocate one page for frame (page fault -> no this page at phys memo )
        vaddr_t vpage =(vaddr_t) alloc_kpages(1);
        if (vpage == 0) return ENOMEM;
        // paddr_t pframe = kvaddr_to_paddr(vpage);
        bzero((void *)vpage, PAGE_SIZE);

        paddr_t pframe = kvaddr_to_paddr(vpage) & PAGE_FRAME;

        if (tregion->writeable != 0) pframe = pframe | TLBLO_DIRTY;

        as->pt[fbits][mbits] = pframe | TLBLO_VALID;

    }

    // store this entry: p_addr, dirty bit, valid bit
    //else {
        // if it is in pagetable, but not in the TLB, we only need to load it to the tlb
    //}   
    
    // disable the cpu interrupts and randomly add this to tlb by tlb_random(entryhi, entrylo)
    int spl = splhigh();
    tlb_random(faultaddress & PAGE_FRAME, as->pt[fbits][mbits]);
    splx(spl);
    return 0;
    // return EFAULT;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

