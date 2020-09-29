/*
 * This file contains definitions of the ioctls and will be shared by the userland code
 * and the kernel module.
 */

#pragma once

#include <stddef.h>

#include <linux/types.h> // TODO: for userland: use stdint.h
#include <linux/ioctl.h>

#define PROC_PATH				"hyperkraken_ctl"
#define HYPERKRAKEN_PROC_PATH				"/proc/" PROC_PATH

// We define the structs as packed to assure a certain struct layout.
struct __attribute__ ((__packed__)) user_arg_segment {
	uint16_t	selector;
	uint16_t	attrib;
	uint32_t	limit;
	uint64_t	base;
} typedef user_arg_segment;

struct __attribute__ ((__packed__)) user_arg_registers {
	uint64_t	guest_id;
	uint64_t	vcpu_id;

	// General-purpose registers
	uint64_t 	rax;
	uint64_t 	rbx;
	uint64_t 	rcx;
	uint64_t 	rdx;
	uint64_t 	rdi;
	uint64_t 	rsi;
	uint64_t 	r8;
	uint64_t 	r9;
	uint64_t 	r10;
	uint64_t 	r11;
	uint64_t 	r12;
	uint64_t 	r13;
	uint64_t 	r14;
	uint64_t 	r15;
	uint64_t 	rbp;
	uint64_t 	rsp;
	uint64_t	rip;
	
	// Control and System registers
	uint64_t	cr0;
	uint64_t	cr2;
	uint64_t	cr3;
	uint64_t	cr4;
	uint64_t	rflags;
	
	// Segments
	struct user_arg_segment	es;
	struct user_arg_segment	cs;
	struct user_arg_segment	ss;
	struct user_arg_segment	ds;
	struct user_arg_segment	fs;
	struct user_arg_segment	gs;
	struct user_arg_segment	gdtr;
	struct user_arg_segment	ldtr;
	struct user_arg_segment	idtr;
	struct user_arg_segment	tr;
	
	// MSRs
	uint64_t	efer;
	uint64_t	star;
	uint64_t	lstar;
	uint64_t	cstar;
	uint64_t	sfmask;
	uint64_t	kernel_gs_base;
	uint64_t	sysenter_cs;
	uint64_t	sysenter_esp;
	uint64_t	sysenter_eip;
} typedef user_arg_registers;

struct __attribute__ ((__packed__)) user_vcpu_guest_id {
	uint64_t	guest_id;
	uint64_t	vcpu_id;
} typedef user_vcpu_guest_id;

struct __attribute__ ((__packed__)) user_memory_region {
	uint64_t			guest_id;
    uint64_t            userspace_addr;
    uint64_t            guest_addr;
	uint64_t			size;
    int                 is_mmio;
	int                 is_cow;
} typedef user_memory_region;

#define ERROR				-1
#define SUCCESS			0

#define HYPERKRAKEN_IOCTL_MAGIC					0xAA
#define HYPERKRAKEN_IOCTL_CREATE_GUEST			_IOW  (HYPERKRAKEN_IOCTL_MAGIC, 0x0, uint64_t)
#define HYPERKRAKEN_IOCTL_CREATE_VCPU			_IOWR (HYPERKRAKEN_IOCTL_MAGIC, 0x1, user_vcpu_guest_id)
#define HYPERKRAKEN_IOCTL_SET_REGISTERS			_IOWR (HYPERKRAKEN_IOCTL_MAGIC, 0x2, user_arg_registers)
#define HYPERKRAKEN_IOCTL_GET_REGISTERS			_IOWR (HYPERKRAKEN_IOCTL_MAGIC, 0x3, user_arg_registers)
#define HYPERKRAKEN_IOCTL_VCPU_RUN				_IOR  (HYPERKRAKEN_IOCTL_MAGIC, 0x4, user_vcpu_guest_id)
#define HYPERKRAKEN_IOCTL_DESTROY_GUEST			_IOR  (HYPERKRAKEN_IOCTL_MAGIC, 0x6, uint64_t)
#define HYPERKRAKEN_SET_MEMORY_REGION			_IOR  (HYPERKRAKEN_IOCTL_MAGIC, 0x7, user_memory_region)