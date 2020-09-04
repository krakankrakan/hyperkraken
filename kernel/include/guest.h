#pragma once

#include <stddef.h>
#include <linux/list.h>
#include <vmcb.h>

typedef struct internal_vcpu internal_vcpu;

struct internal_vcpu {
	internal_vcpu*		next;
	uint64_t		id;
	uint64_t		physical_core; // the phyiscal core id the vcpu is mapped to
	vmcb*			vcpu_vmcb;
	vmcb*			host_vmcb;
	gp_regs*		vcpu_regs;
} typedef internal_vcpu;

struct internal_guest {
	internal_vcpu* 	vcpus;
	uint64_t		highest_phys_addr; // contains the number of bytes the guest has available as memory
	uint64_t		used_cores;
	void*			nested_pagetables; // map guest physical to host physical memory
	// intercept reasons set in the VMCB for all VCPUs
	uint32_t		intercept_exceptions;
	uint64_t		intercept;
} typedef internal_guest;

extern internal_guest* guest;

internal_vcpu* map_vcpu_id_to_vcpu(uint8_t id, internal_guest* g);
void update_intercept_reasons(internal_guest* g);
