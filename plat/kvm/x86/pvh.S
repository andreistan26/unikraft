/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2022, Unikraft GmbH and The Unikraft Authors.
 * Licensed under the BSD-3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 */

#include <uk/config.h>
#include <uk/asm.h>
#include <uk/plat/common/lcpu.h>
#include <uk/reloc.h>
#include <kvm-x86/pvh.h>

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

ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY, .long _pvh_entry)

/**
 * Stack and entry function to use during CPU initialization
 */
.section .bss
.space 4096
lcpu_bootstack:

.section .rodata
lcpu_boot_startup_args:
	ur_data	quad, pvh_entry, 8
	ur_data	quad, lcpu_bootstack, 8

/**
 * 64-bit Linux Boot Protocol entry function
 *
 * RSI holds base address of struct lxboot_params. Flat 4GiB CS and DS segments,
 * with ES and SS set to DS. A20 gate enabled. Protected mode enabled, paging
 * enabled with ident mapping for loaded kernel, zero page (i.e., struct
 * lxboot_params) and command line buffer. Interrupts disabled.
 */
.code32
.section .text.32.boot
ENTRY(_pvh_entry)
	cmpl	$PVH_START_MAGIC_VALUE, 0(%ebx)
	jne	no_pvh

	/* FIXME: relocation support */

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
END(_pvh_entry)
