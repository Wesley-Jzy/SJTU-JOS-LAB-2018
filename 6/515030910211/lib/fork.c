// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR)) {
		panic("pgfault(): access should be a write\n");
	}

	if ( !( (vpd[PDX(addr)] & PTE_P) && 
		    (vpt[PGNUM(addr)] & PTE_P) &&
		    (vpt[PGNUM(addr)] & PTE_COW) )) {
		panic("pgfault(): access should be to a COW\n");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W);
	if (r < 0){
		panic("pgfault: %e", r);
	}

	memmove(PFTEMP, addr, PGSIZE);

	r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W);
	if (r < 0){
		panic("pgfault: %e", r);
	}

	r = sys_page_unmap(0, PFTEMP);
	if (r < 0){
		panic("pgfault: %e", r);
	}

	//panic("pgfault not implemented");

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	void *addr = (void *)(pn * PGSIZE);
	if ((vpd[PDX(addr)] & PTE_P) && (vpt[pn] & PTE_P)) {
		if (vpt[pn] & (PTE_W | PTE_COW)) {
			r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW);
			if (r < 0) {
				panic("duppage: %e", r);
			}

			/* because parent's write should be seen by forks */
			r = sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW);
			if (r < 0) {
				panic("duppage: %e", r);
			}
		} else {
			r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U);
			if (r < 0) {
				panic("duppage: %e", r);
			}
		}
	} 

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	/* Set up our page fault handler appropriately */
	int r;
	extern void _pgfault_upcall(void);

	set_pgfault_handler(&pgfault);

	/* Create a child */ 
	envid_t envid = sys_exofork();

	if (envid < 0) {
		panic("fork: %e", envid);
	}

	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
    	return 0;
	}

	/* Copy our address space and page fault handler setup to the child */
	unsigned pn;
	uint8_t *addr;

	for (addr = 0; addr < (uint8_t*) USTACKTOP; addr += PGSIZE) {
		if ((vpd[PDX(addr)] & PTE_P) && 
			(vpt[PGNUM(addr)] & (PTE_P | PTE_U)) == (PTE_P | PTE_U)) {
			pn = PGNUM(addr);
			r = duppage(envid, pn);
			if (r < 0) {
				panic("fork: %e", r);
			}
		}
	}

	/* allocate a new page for the child's user exception stack */
	r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if (r < 0) {
		panic("fork: %e", r);
	} 


	r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if (r < 0) {
		panic("fork: %e", r);
	}

	/* Then mark the child as runnable and return */
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0) {
		panic("fork: %e", r);
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
