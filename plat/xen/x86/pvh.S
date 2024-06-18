/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2022, Unikraft GmbH and The Unikraft Authors.
 * Licensed under the BSD-3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 */

#include <uk/config.h>
#include <uk/asm.h>
#include <uk/plat/common/lcpu.h>
#include <uk/arch/lcpu.h>
#include <uk/reloc.h>
#include <xen-x86/pvh.h>
#include <xen/elfnote.h>

#include <x86/traps.h>
#include <uk/asm/cfi.h>
#include <xen-x86/traps.h>


#define ELFNOTE(name, type, desc)               \
	.pushsection .note.name               ; \
	.align 4                              ; \
	.long 2f - 1f       /* namesz */      ; \
	.long 4f - 3f       /* descsz */      ; \
	.long type          /* type   */      ; \
1:	.asciz #name        /* name   */      ; \
2:	.align 4                              ; \
3	:desc               /* desc   */      ; \
4:	.align 4                              ; \
	.popsection

#define XEN_ELFNOTE_PHYS32_ENTRY 18
#define XEN_ELFNOTE_FEATURES     10

ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY, .long _libxenplat_start)
ELFNOTE(Xen, XEN_ELFNOTE_FEATURES, .asciz "auto_translated_physmap|supervisor_mode_kernel|hvm_callback_vector")

/**
 * Stack and entry function to use during CPU initialization
 */
.section .bss
.space 4096
lcpu_bootstack:

.section .data
.globl _libxenplat_shared_info
        .align __PAGE_SIZE
_libxenplat_shared_info:
        .fill __PAGE_SIZE,1,0

.section .rodata
lcpu_boot_startup_args:
	ur_data	quad, libxenplat_start, 8
	ur_data	quad, lcpu_bootstack, 8

.code32
.section .text.32.boot
ENTRY(_libxenplat_start)
	cmpl	$PVH_START_MAGIC_VALUE, 0(%ebx)
	jne	no_pvh

	/* FIXME: relocation support */
	movl    $0x00000000, %esi
	do_uk_reloc32	0

	/* startup args for boot CPU */
	ur_mov  lcpu_boot_startup_args, %edi, 4, _phys
	movl	%ebx, %esi			/* PVH info */

	ur_mov lcpu_start32, %ebx, 4, _phys
	jmp *%ebx

no_pvh:
	cli
1:
	hlt
	jmp	1b
END(_libxenplat_start)

.altmacro
.code64
.section .text.boot

.globl hypercall_page
        .align __PAGE_SIZE
hypercall_page:
        .fill __PAGE_SIZE,1,0

.macro PUSH_CALLER_SAVE
	pushq_reg_cfi rdi
	pushq_reg_cfi rsi
	pushq_reg_cfi rdx
	pushq_reg_cfi rcx
	pushq_reg_cfi rax
	pushq_reg_cfi r8
	pushq_reg_cfi r9
	pushq_reg_cfi r10
	pushq_reg_cfi r11
	pushq_reg_cfi rbx
	pushq_reg_cfi rbp
	pushq_reg_cfi r12
	pushq_reg_cfi r13
	pushq_reg_cfi r14
	pushq_reg_cfi r15
.endm

.macro POP_CALLER_SAVE
	popq_reg_cfi r15
	popq_reg_cfi r14
	popq_reg_cfi r13
	popq_reg_cfi r12
	popq_reg_cfi rbp
	popq_reg_cfi rbx
	popq_reg_cfi r11
	popq_reg_cfi r10
	popq_reg_cfi r9
	popq_reg_cfi r8
	popq_reg_cfi rax
	popq_reg_cfi rcx
	popq_reg_cfi rdx
	popq_reg_cfi rsi
	popq_reg_cfi rdi
.endm

.macro TRAP_ENTRY trapname, has_ec
ENTRY(ASM_TRAP_SYM(\trapname))
	.cfi_startproc simple
	.cfi_signal_frame
	.cfi_def_cfa rsp, 0
.if \has_ec
	/* Error code is pushed on the stack after the return address */
	.cfi_def_cfa_offset 16
.else
	/* There is only the return address on the stack */
	.cfi_def_cfa_offset 8
.endif
	.cfi_offset rip, -8
	/* Description of the stack with active IST */
	.cfi_offset cs, 0

#ifdef __clang__
	.cfi_offset flags, 8
#elif __GNUC__
	.cfi_offset rflags, 8
#else
#error "Unsupported Compiler"
#endif

	.cfi_offset rsp, 16
	.cfi_offset ss, 24
	cld

.if !\has_ec
	pushq_cfi $0                        /* no error code, pass 0 */
.endif
	PUSH_CALLER_SAVE
	subq $__REGS_PAD_SIZE, %rsp         /* we have some padding */
	.cfi_adjust_cfa_offset __REGS_PAD_SIZE

	movq %rsp, %rdi
	movq __REGS_OFFSETOF_ORIG_RAX(%rsp), %rsi
	call do_\trapname

	addq $__REGS_PAD_SIZE, %rsp         /* we have some padding */
	.cfi_adjust_cfa_offset -__REGS_PAD_SIZE
	POP_CALLER_SAVE
	addq $8, %rsp                       /* discard error code */
	.cfi_adjust_cfa_offset -8

	iretq
	.cfi_endproc
.endm


TRAP_ENTRY divide_error,        0
TRAP_ENTRY debug,               0
/* no NMI */
TRAP_ENTRY int3,                0
TRAP_ENTRY overflow,            0
TRAP_ENTRY bounds,              0
TRAP_ENTRY invalid_op,          0
TRAP_ENTRY no_device,           0
/* no Double Fault */
TRAP_ENTRY coproc_seg_overrun,  0
TRAP_ENTRY invalid_tss,         1
TRAP_ENTRY no_segment,          1
TRAP_ENTRY stack_error,         1 /* runs on exception stack */
TRAP_ENTRY gp_fault,            1
TRAP_ENTRY page_fault,          1
TRAP_ENTRY spurious_int,        1
TRAP_ENTRY coproc_error,        0
TRAP_ENTRY alignment_check,     1
/* no Machine Check */
TRAP_ENTRY simd_error,          0

/* serves TRAP_xen_callback */
TRAP_ENTRY hypervisor_callback  0

.noaltmacro
