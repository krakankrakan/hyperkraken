#include <svm/svm.h>
#include <svm/svm_ops.h>
#include <guest.h>
#include <memory.h>
#include <stddef.h>
#include <mah.h>

#include <linux/slab.h>

void* svm_create_arch_internal_guest(internal_guest* g) {
	svm_internal_guest* svm_g;
	unsigned int i;

	svm_g = (svm_internal_guest*) kzalloc(sizeof(internal_guest), GFP_KERNEL);
	TEST_PTR(svm_g, svm_internal_guest*,,NULL)
	
	// Get the root for the nested pagetables from the MMU.
	svm_g->nested_pagetables = g->mmu->base;

	// SVM offers the possibility to intercept MSR instructions via a 
	// SVM MSR permissions map (MSR). Each MSR is covered by two bits,
	// the lsb controls read access and the msb controls write acccess.
	// The MSR bitmap consists of 4 bit vectors of 2kB each.
	// MSR bitmap offset        MSR range
	// 0x0      - 0x7FFF:        0x0        - 0x1FFF
	// 0x800    - 0xFFFF:        0xC0000000 - 0xC0001FFF
	// 0x1000   - 0x17FFF:       0xC0010000 - 0xC0011FFF
	// 0x1800   - 0x1FFFF:       Reserved
	svm_g->msr_permission_map = (uint8_t*) kzalloc(MSRPM_SIZE, GFP_KERNEL);
	TEST_PTR(svm_g->msr_permission_map, uint8_t*, kfree(svm_g); kfree(svm_g->nested_pagetables), NULL)

	for(i = 0; i < MSRPM_SIZE; i++) svm_g->msr_permission_map[i] = 0;

	// We only allow direct access to a few selected MSRs.
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_STAR, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_LSTAR, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_CSTAR, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_SYSENTER_CS, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_SYSENTER_ESP, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_SYSENTER_EIP, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_GS_BASE, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_FS_BASE, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_KERNEL_GS_BASE, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_SYSCALL_MASK, 1, 1);
	/*svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_SPEC_CTRL, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_PRED_CMD, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_LASTBRANCHFROMIP, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_LASTBRANCHTOIP, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_LASTINTFROMIP, 1, 1);
	svm_set_msrpm_permission(svm_g->msr_permission_map, MSR_IA32_LASTINTTOIP, 1, 1);*/

	return (void*)svm_g;
}

void* svm_create_arch_internal_vcpu(internal_guest* g) {
	svm_internal_guest* svm_g;
	svm_internal_vcpu* svm_vcpu;

	TEST_PTR(g, internal_guest*,, NULL)
	svm_g = to_svm_guest(g);
	TEST_PTR(svm_g, svm_internal_guest*,, NULL)

	// TODO: Test if creating a VCPU exceedes the phyiscal cores on the system
	
	svm_vcpu = kzalloc(sizeof(internal_vcpu), GFP_KERNEL);
	
	svm_vcpu->vcpu_vmcb = kzalloc(PAGE_SIZE, GFP_KERNEL);
	svm_vcpu->host_vmcb = kzalloc(PAGE_SIZE, GFP_KERNEL);
	svm_vcpu->vcpu_regs = kzalloc(sizeof(gp_regs), GFP_KERNEL);
	
	TEST_PTR(svm_vcpu->vcpu_vmcb, vmcb*, kfree(svm_vcpu), NULL);
	TEST_PTR(svm_vcpu->host_vmcb, vmcb*, kfree(svm_vcpu); kfree(svm_vcpu->vcpu_vmcb), NULL);
	TEST_PTR(svm_vcpu->vcpu_regs, gp_regs*, kfree(svm_vcpu); kfree(svm_vcpu->vcpu_vmcb); kfree(svm_vcpu->host_vmcb), NULL);

	svm_reset_vcpu(svm_vcpu, g);

	return (void*)svm_vcpu;
}

int svm_destroy_arch_internal_vcpu(internal_vcpu* vcpu) {
	svm_internal_vcpu* svm_vcpu;
	
	TEST_PTR(vcpu, internal_vcpu*,, ERROR);

	svm_vcpu = to_svm_vcpu(vcpu);

	if (svm_vcpu != NULL) {
		if (svm_vcpu->vcpu_vmcb != NULL) kfree(svm_vcpu->vcpu_vmcb);
		if (svm_vcpu->host_vmcb != NULL) kfree(svm_vcpu->host_vmcb);
		if (svm_vcpu->vcpu_regs != NULL) kfree(svm_vcpu->vcpu_regs);
		return SUCCESS;
	}
	return ERROR;
}

void svm_destroy_arch_internal_guest(internal_guest* g) {
	svm_internal_guest* svm_g;

	TEST_PTR(g, internal_guest*,,)
	svm_g = to_svm_guest(g);
	TEST_PTR(svm_g, svm_internal_guest*,,)

	// If we are here, we can assume that all locks are set.
	if (svm_g->msr_permission_map != NULL) kfree(svm_g->msr_permission_map);
	if (svm_g->io_permission_map != NULL)  kfree(svm_g->io_permission_map);
}

void svm_set_vcpu_registers(internal_vcpu* vcpu, user_arg_registers* regs) {
	svm_internal_vcpu* svm_vcpu;
	
	TEST_PTR(vcpu, internal_vcpu*,,);
	TEST_PTR(regs, user_arg_registers*,,);

	svm_vcpu = to_svm_vcpu(vcpu);
	TEST_PTR(svm_vcpu, svm_internal_vcpu*,,);

	TEST_PTR(svm_vcpu->vcpu_vmcb, vmcb*,,);
	TEST_PTR(svm_vcpu->vcpu_regs, gp_regs*,,);
	
	svm_vcpu->vcpu_vmcb->rax = regs->rax;
	svm_vcpu->vcpu_vmcb->rsp = regs->rsp;
	svm_vcpu->vcpu_vmcb->rip = regs->rip;
	
	svm_vcpu->vcpu_vmcb->cr0 = regs->cr0;
	svm_vcpu->vcpu_vmcb->cr2 = regs->cr2;
	svm_vcpu->vcpu_vmcb->cr3 = regs->cr3;
	svm_vcpu->vcpu_vmcb->cr4 = regs->cr4;
	
	svm_vcpu->vcpu_vmcb->efer   = regs->efer;
	svm_vcpu->vcpu_vmcb->star   = regs->star;
	svm_vcpu->vcpu_vmcb->lstar  = regs->lstar;
	svm_vcpu->vcpu_vmcb->cstar  = regs->cstar;
	svm_vcpu->vcpu_vmcb->sfmask = regs->sfmask;
	svm_vcpu->vcpu_vmcb->kernel_gs_base = regs->kernel_gs_base;
	svm_vcpu->vcpu_vmcb->sysenter_cs    = regs->sysenter_cs;
	svm_vcpu->vcpu_vmcb->sysenter_esp   = regs->sysenter_esp;
	svm_vcpu->vcpu_vmcb->sysenter_eip   = regs->sysenter_eip;
	
	svm_vcpu->vcpu_regs->rbx = regs->rbx;
	svm_vcpu->vcpu_regs->rcx = regs->rcx;
	svm_vcpu->vcpu_regs->rdx = regs->rdx;
	svm_vcpu->vcpu_regs->rdi = regs->rdi;
	svm_vcpu->vcpu_regs->rsi = regs->rsi;
	svm_vcpu->vcpu_regs->r8  = regs->r8;
	svm_vcpu->vcpu_regs->r9  = regs->r9;
	svm_vcpu->vcpu_regs->r10 = regs->r10;
	svm_vcpu->vcpu_regs->r11 = regs->r11;
	svm_vcpu->vcpu_regs->r12 = regs->r12;
	svm_vcpu->vcpu_regs->r13 = regs->r13;
	svm_vcpu->vcpu_regs->r14 = regs->r14;
	svm_vcpu->vcpu_regs->r15 = regs->r15;
	svm_vcpu->vcpu_regs->rbp = regs->rbp;
	
	memcpy(&svm_vcpu->vcpu_vmcb->es, &regs->es, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->cs, &regs->cs, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->ss, &regs->ss, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->ds, &regs->ds, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->fs, &regs->fs, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->gs, &regs->gs, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->gdtr, &regs->gdtr, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->ldtr, &regs->ldtr, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->idtr, &regs->idtr, sizeof(segment));
	memcpy(&svm_vcpu->vcpu_vmcb->tr, &regs->tr, sizeof(segment));
}

void svm_get_vcpu_registers(internal_vcpu* vcpu, user_arg_registers* regs) {
	svm_internal_vcpu* svm_vcpu;
	
	TEST_PTR(vcpu, internal_vcpu*,,);
	TEST_PTR(regs, user_arg_registers*,,);

	svm_vcpu = to_svm_vcpu(vcpu);
	TEST_PTR(svm_vcpu, svm_internal_vcpu*,,);

	TEST_PTR(svm_vcpu->vcpu_vmcb, vmcb*,,);
	TEST_PTR(svm_vcpu->vcpu_regs, gp_regs*,,);
	
	regs->rax = svm_vcpu->vcpu_vmcb->rax;
	regs->rsp = svm_vcpu->vcpu_vmcb->rsp;
	regs->rip = svm_vcpu->vcpu_vmcb->rip;
	
	regs->cr0 = svm_vcpu->vcpu_vmcb->cr0;
	regs->cr2 = svm_vcpu->vcpu_vmcb->cr2;
	regs->cr3 = svm_vcpu->vcpu_vmcb->cr3;
	regs->cr4 = svm_vcpu->vcpu_vmcb->cr4;
	
	regs->efer   = svm_vcpu->vcpu_vmcb->efer;
	regs->star   = svm_vcpu->vcpu_vmcb->star;
	regs->lstar  = svm_vcpu->vcpu_vmcb->lstar;
	regs->cstar  = svm_vcpu->vcpu_vmcb->cstar;
	regs->sfmask = svm_vcpu->vcpu_vmcb->sfmask;
	regs->kernel_gs_base = svm_vcpu->vcpu_vmcb->kernel_gs_base;
	regs->sysenter_cs    = svm_vcpu->vcpu_vmcb->sysenter_cs;
	regs->sysenter_esp   = svm_vcpu->vcpu_vmcb->sysenter_esp;
	regs->sysenter_eip   = svm_vcpu->vcpu_vmcb->sysenter_eip;
	
	regs->rbx = svm_vcpu->vcpu_regs->rbx;
	regs->rcx = svm_vcpu->vcpu_regs->rcx;
	regs->rdx = svm_vcpu->vcpu_regs->rdx;
	regs->rdi = svm_vcpu->vcpu_regs->rdi;
	regs->rsi = svm_vcpu->vcpu_regs->rsi;
	regs->r8  = svm_vcpu->vcpu_regs->r8;
	regs->r9  = svm_vcpu->vcpu_regs->r9;
	regs->r10 = svm_vcpu->vcpu_regs->r10;
	regs->r11 = svm_vcpu->vcpu_regs->r11;
	regs->r12 = svm_vcpu->vcpu_regs->r12;
	regs->r13 = svm_vcpu->vcpu_regs->r13;
	regs->r14 = svm_vcpu->vcpu_regs->r14;
	regs->r15 = svm_vcpu->vcpu_regs->r15;
	regs->rbp = svm_vcpu->vcpu_regs->rbp;
	
	memcpy(&regs->es, &svm_vcpu->vcpu_vmcb->es, sizeof(segment));
	memcpy(&regs->cs, &svm_vcpu->vcpu_vmcb->cs, sizeof(segment));
	memcpy(&regs->ss, &svm_vcpu->vcpu_vmcb->ss, sizeof(segment));
	memcpy(&regs->ds, &svm_vcpu->vcpu_vmcb->ds, sizeof(segment));
	memcpy(&regs->fs, &svm_vcpu->vcpu_vmcb->fs, sizeof(segment));
	memcpy(&regs->gs, &svm_vcpu->vcpu_vmcb->gs, sizeof(segment));
	memcpy(&regs->gdtr, &svm_vcpu->vcpu_vmcb->gdtr, sizeof(segment));
	memcpy(&regs->ldtr, &svm_vcpu->vcpu_vmcb->ldtr, sizeof(segment));
	memcpy(&regs->idtr, &svm_vcpu->vcpu_vmcb->idtr, sizeof(segment));
	memcpy(&regs->tr, &svm_vcpu->vcpu_vmcb->tr, sizeof(segment));
}

void svm_set_memory_region(internal_guest* g, internal_memory_region* memory_region) {
	// TODO
}

uint64_t svm_map_page_attributes_to_arch(uint64_t attrib) {
	uint64_t 	ret;

	if ((attrib & ~PAGE_ATTRIB_WRITE) != 0) 		ret |= _PAGE_RW;
	if ((attrib & ~PAGE_ATTRIB_EXEC) == 0) 		ret |= _PAGE_NX;
	if ((attrib & ~PAGE_ATTRIB_PRESENT) != 0) 	ret |= _PAGE_PRESENT;
	if ((attrib & ~PAGE_ATTRIB_DIRTY) != 0) 		ret |= _PAGE_DIRTY;
	if ((attrib & ~PAGE_ATTRIB_ACCESSED) != 0) 	ret |= _PAGE_ACCESSED;
	if ((attrib & ~PAGE_ATTRIB_HUGE) != 0) 		ret |= _PAGE_SPECIAL;
	
	// For AMD SVM, a nested page always has to be a user page
	ret |= _PAGE_USER;

	return ret;
}

uint64_t svm_map_arch_to_page_attributes(uint64_t attrib) {
	uint64_t 	ret = 0;

	// On x86, a page is always readable
	ret |= PAGE_ATTRIB_READ;
	
	if ((attrib & ~_PAGE_RW) != 0) 		ret |= PAGE_ATTRIB_WRITE;
	if ((attrib & ~_PAGE_NX) == 0) 		ret |= PAGE_ATTRIB_EXEC;
	if ((attrib & ~_PAGE_PRESENT) != 0) 	ret |= PAGE_ATTRIB_PRESENT;
	if ((attrib & ~_PAGE_DIRTY) != 0) 	ret |= PAGE_ATTRIB_DIRTY;
	if ((attrib & ~_PAGE_ACCESSED) != 0) 	ret |= PAGE_ATTRIB_ACCESSED;
	if ((attrib & ~_PAGE_USER) != 0) 		ret |= PAGE_ATTRIB_USER;
	if ((attrib & ~_PAGE_SPECIAL) != 0)	ret |= PAGE_ATTRIB_HUGE;

	return ret;
}

void svm_init_mmu(internal_mmu* m) {
	m->levels = 4;
	m->base = kzalloc(PAGE_SIZE, GFP_KERNEL);
	INIT_LIST_HEAD(&m->pagetables_list);
	INIT_LIST_HEAD(&m->memory_region_list);
}

void svm_destroy_mmu(internal_mmu* m) {
	if (m->base) kfree(m->base);
}

// The following functions implement 4-level nested pages (4KB pages) for SVM
uint64_t svm_get_vpn_from_level(uint64_t virt_addr, unsigned int level) {
    return (virt_addr >> ((level-1)*9 + 12)) & (uint64_t)0x1ff;
}

int svm_mmu_walk_available(hpa_t *pte, gpa_t phys_guest, unsigned int *current_level) {
	if ((*pte & PAGE_TABLE_MASK) == 0) return ERROR;
	else return SUCCESS;
}

hpa_t* svm_mmu_walk_next(hpa_t *pte, gpa_t phys_guest, unsigned int *current_level) {
	uint64_t	vpn = svm_get_vpn_from_level(phys_guest, *current_level);
	*current_level  = *current_level - 1;
	return (hpa_t*)__va((((hpa_t*)(*pte))[vpn]) & PAGE_TABLE_MASK);
}

hpa_t* svm_mmu_walk_init(internal_mmu *m, gpa_t phys_guest, unsigned int *current_level) {
	uint64_t	vpn = svm_get_vpn_from_level(phys_guest, *current_level);
	*current_level  = m->levels;
	return &(m->base[vpn]);
}

// This function will be called if AMD SVM support is detected
void init_svm_mah_ops(void) {
	mah_ops.run_vcpu 					= svm_run_vcpu;
    mah_ops.create_arch_internal_vcpu 	= svm_create_arch_internal_vcpu,
	mah_ops.destroy_arch_internal_vcpu 	= svm_destroy_arch_internal_vcpu,
    mah_ops.create_arch_internal_guest 	= svm_create_arch_internal_guest;
    mah_ops.destroy_arch_internal_guest = svm_destroy_arch_internal_guest;
	mah_ops.set_vcpu_registers 			= svm_set_vcpu_registers;
    mah_ops.get_vcpu_registers 			= svm_get_vcpu_registers;
    mah_ops.set_memory_region 			= svm_set_memory_region;
	mah_ops.map_page_attributes_to_arch	= svm_map_page_attributes_to_arch;
	mah_ops.map_arch_to_page_attributes	= svm_map_arch_to_page_attributes;
	mah_ops.init_mmu					= svm_init_mmu;
	mah_ops.destroy_mmu					= svm_destroy_mmu;
	mah_ops.mmu_walk_available			= svm_mmu_walk_available;
	mah_ops.mmu_walk_next				= svm_mmu_walk_next;
	mah_ops.mmu_walk_init				= svm_mmu_walk_init;

	mah_initialized = 1;
}