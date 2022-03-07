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
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	void* alignment_address = (void*)ROUNDDOWN(utf->utf_fault_va, PGSIZE);
	uint64_t pn = ((uint64_t)addr) / PGSIZE;
	
	if (!((err & FEC_WR) && (uvpt[pn] & PTE_COW)))
		panic("pgfault: not page for write and COW fault!");


	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	envid_t cur_id = sys_getenvid();
	if ((r = sys_page_alloc(cur_id, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault: page fault handler will allocate at %x in : %e", addr, r);
	
	memcpy(PFTEMP, alignment_address, PGSIZE);
	
	if ((r = sys_page_map(cur_id, PFTEMP, cur_id, alignment_address, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault: page fault handler will move %x  to PFTEMP in : %e", addr, r);
	
	if ((r = sys_page_unmap(cur_id, PFTEMP)) < 0)
		panic("pgfault: unmap PFTEMP failed! %e", r);

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
	uint64_t pn_64 = pn;
	void* addr = (void*)(pn_64 * PGSIZE);
  
	envid_t cur_id = sys_getenvid();
	int perm = uvpt[pn_64] & 0xfff;
	
	if ((uvpt[pn_64] & PTE_W) || (uvpt[pn_64] & PTE_COW))
	{
		if ((r = sys_page_map(cur_id, addr, envid, addr, PTE_P | PTE_U | PTE_COW)) < 0)
			panic("duppage: mapping 0x%x failed!: %e", addr, r);
		if ((r = sys_page_map(cur_id, addr, cur_id, addr, PTE_P | PTE_U | PTE_COW)) < 0)
			panic("duppage: mapping 0x%x failed!: %e", addr, r);
	}
	else
	{
		if ((r = sys_page_map(cur_id, addr, envid, addr, PTE_P | PTE_U | perm)) < 0)
			panic("duppage: map %x failed!: %e", addr, r);
	}
	
	//panic("duppage not implemented");
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
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	envid_t child_env;
	uintptr_t addr;
	envid_t cur_id = sys_getenvid();
	set_pgfault_handler(pgfault);
	child_env = sys_exofork();
	
	if (child_env < 0)
		panic("fork: creating new env failed!\n");
	if (child_env == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	
	if (sys_page_alloc(child_env, (void*)(UXSTACKTOP-PGSIZE), PTE_P | PTE_U | PTE_W) < 0)
		panic("fork: alloc exception stack failed!");
	
	extern void _pgfault_upcall(void);
	if (sys_env_set_pgfault_upcall(child_env, _pgfault_upcall) < 0)
		panic("fork: upcall failed!");
	
	extern unsigned char end[];
	for (addr = UTEXT; addr < (uintptr_t)end; addr += PGSIZE)
		if ((uvpml4e[VPML4E(addr)] & PTE_P) && (uvpde[VPDPE(addr)] & PTE_P) && (uvpd[VPD(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & (PTE_P | PTE_U)))
			duppage(child_env, PGNUM(addr));
	
	if (sys_page_alloc(cur_id, PFTEMP, PTE_P|PTE_U|PTE_W) < 0)
		panic("fork: allocating PFTEMP failed!");
	memcpy((void*)PFTEMP, (void*)(USTACKTOP - PGSIZE), PGSIZE);
	
	if (sys_page_map(cur_id, PFTEMP, child_env, (void*)(USTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P) < 0)
 		panic("fork: map PFTEMP to child's stack failed!");
	
	if (sys_page_unmap(cur_id, (void*)PFTEMP) < 0)
		panic("fork: ummap PFTEMP failed!");
	
	if ((sys_env_set_status(child_env, ENV_RUNNABLE)) < 0)
		panic("fork: set runnable failed!\n");
	

return child_env;
	
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
