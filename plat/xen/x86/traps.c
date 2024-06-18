/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Taken from Mini-OS */

#include <stddef.h>
#include <xen-x86/traps.h>
#include <xen-x86/hypercall.h>
#include <uk/print.h>
#include <uk/plat/common/lcpu.h>

#if ! defined(XEN_PARAVIRT)
#include <uk/arch/ctx.h>
#include <string.h>
#include <uk/essentials.h>
#include <uk/arch/lcpu.h>
#include <uk/arch/paging.h>
#include <uk/plat/lcpu.h>
#include <uk/plat/config.h>
#include <x86/desc.h>
#include <common/hypervisor.h>
#include <xen/hvm/params.h>
#endif

/* Traps used only on Xen */
DECLARE_TRAP_EC(coproc_seg_overrun, "coprocessor segment overrun", NULL)
DECLARE_TRAP   (spurious_int,       "spurious interrupt bug",      NULL)


#if defined(XEN_PARAVIRT)

#define TRAP_TABLE_ENTRY(trapname, pl) \
	{ TRAP_##trapname, pl, __KERNEL_CS, (unsigned long) ASM_TRAP_SYM(trapname) }

/*
 * Submit a virtual IDT to the hypervisor. This consists of tuples
 * (interrupt vector, privilege ring, CS:EIP of handler).
 * The 'privilege ring' field specifies the least-privileged ring that
 * can trap to that vector using a software-interrupt instruction (INT).
 */
static trap_info_t trap_table[] = {
	TRAP_TABLE_ENTRY(divide_error,        0),
	TRAP_TABLE_ENTRY(debug,               0),
	TRAP_TABLE_ENTRY(int3,                3),
	TRAP_TABLE_ENTRY(overflow,            3),
	TRAP_TABLE_ENTRY(bounds,              3),
	TRAP_TABLE_ENTRY(invalid_op,          0),
	TRAP_TABLE_ENTRY(no_device,           0),
	TRAP_TABLE_ENTRY(coproc_seg_overrun,  0),
	TRAP_TABLE_ENTRY(invalid_tss,         0),
	TRAP_TABLE_ENTRY(no_segment,          0),
	TRAP_TABLE_ENTRY(stack_error,         0),
	TRAP_TABLE_ENTRY(gp_fault,            0),
	TRAP_TABLE_ENTRY(page_fault,          0),
	TRAP_TABLE_ENTRY(spurious_int,        0),
	TRAP_TABLE_ENTRY(coproc_error,        0),
	TRAP_TABLE_ENTRY(alignment_check,     0),
	TRAP_TABLE_ENTRY(simd_error,          0),
	{ 0, 0, 0, 0 }
};

void traps_lcpu_init(struct lcpu *current __unused)
{
	HYPERVISOR_set_trap_table(trap_table);

	HYPERVISOR_set_callbacks((unsigned long) asm_trap_hypervisor_callback,
				 (unsigned long) asm_failsafe_callback, 0);
}

#else

/* Mostly duplicated from plat/kvm */

__align(UKARCH_SP_ALIGN) /* IST1 */
char cpu_intr_stack[CONFIG_UKPLAT_LCPU_MAXCOUNT][CPU_EXCEPT_STACK_SIZE];
__align(UKARCH_SP_ALIGN) /* IST2 */
char cpu_trap_stack[CONFIG_UKPLAT_LCPU_MAXCOUNT][CPU_EXCEPT_STACK_SIZE];
__align(UKARCH_SP_ALIGN) /* IST3 */
char cpu_crit_stack[CONFIG_UKPLAT_LCPU_MAXCOUNT][CPU_EXCEPT_STACK_SIZE];

static __align(8)
struct tss64 cpu_tss[CONFIG_UKPLAT_LCPU_MAXCOUNT];

static __align(8)
struct seg_desc32 cpu_gdt64[CONFIG_UKPLAT_LCPU_MAXCOUNT][GDT_NUM_ENTRIES];

static void gdt_init(__lcpuidx idx)
{
	volatile struct desc_table_ptr64 gdtptr; /* needs to be volatile so
						  * setting its values is not
						  * optimized out
						  */

	cpu_gdt64[idx][GDT_DESC_CODE].raw = GDT_DESC_CODE64_VAL;
	cpu_gdt64[idx][GDT_DESC_DATA].raw = GDT_DESC_DATA64_VAL;

	gdtptr.limit = sizeof(cpu_gdt64[idx]) - 1;
	gdtptr.base = (__u64) &cpu_gdt64[idx];

	__asm__ goto(
		/* Load the global descriptor table */
		"lgdt	%0\n"

		/* Perform a far return to enable the new CS */
		"leaq	%l[jump_to_new_cs](%%rip), %%rax\n"

		"pushq	%1\n"
		"pushq	%%rax\n"
		"lretq\n"
		:
		: "m"(gdtptr),
		  "i"(GDT_DESC_OFFSET(GDT_DESC_CODE))
		: "rax", "memory" : jump_to_new_cs);
jump_to_new_cs:

	__asm__ __volatile__(
		/* Update remaining segment registers */
		"movl	%0, %%es\n"
		"movl	%0, %%ss\n"
		"movl	%0, %%ds\n"

		/* Initialize fs and gs to 0 */
		"movl	%1, %%fs\n"
		"movl	%1, %%gs\n"
		:
		: "r"(GDT_DESC_OFFSET(GDT_DESC_DATA)),
		  "r"(0));
}

static void tss_init(__lcpuidx idx)
{
	struct seg_desc64 *tss_desc;

	cpu_tss[idx].ist[0] =
		(__u64) &cpu_intr_stack[idx][sizeof(cpu_intr_stack[idx])];
	cpu_tss[idx].ist[1] =
		(__u64) &cpu_trap_stack[idx][sizeof(cpu_trap_stack[idx])];
	cpu_tss[idx].ist[2] =
		(__u64) &cpu_crit_stack[idx][sizeof(cpu_crit_stack[idx])];

	tss_desc = (void *) &cpu_gdt64[idx][GDT_DESC_TSS_LO];
	tss_desc->limit_lo	= sizeof(cpu_tss[idx]);
	tss_desc->base_lo	= (__u64) &(cpu_tss[idx]);
	tss_desc->base_hi	= (__u64) &(cpu_tss[idx]) >> 24;
	tss_desc->type		= GDT_DESC_TYPE_TSS_AVAIL;
	tss_desc->p		= 1;

	__asm__ __volatile__(
		"ltr	%0\n"
		:
		: "r"((__u16) (GDT_DESC_OFFSET(GDT_DESC_TSS_LO))));
}


static struct seg_gate_desc64 cpu_idt[IDT_NUM_ENTRIES] __align(8);
static struct desc_table_ptr64 idtptr;

static inline void idt_fillgate(unsigned int num, void *fun, unsigned int ist)
{
	struct seg_gate_desc64 *desc = &cpu_idt[num];

	/*
	 * All gates are interrupt gates, all handlers run with interrupts off.
	 */
	desc->offset_hi	= (__u64) fun >> 16;
	desc->offset_lo	= (__u64) fun & 0xffff;
	desc->selector	= IDT_DESC_OFFSET(IDT_DESC_CODE);
	desc->ist	= ist;
	desc->type	= IDT_DESC_TYPE_INTR;
	desc->dpl	= IDT_DESC_DPL_KERNEL;
	desc->p		= 1;
}

static void idt_init(void)
{
	/* Ensure that traps_table_init has been called */
	UK_ASSERT(idtptr.limit != 0);

	__asm__ __volatile__("lidt %0" :: "m" (idtptr));
}

void traps_table_init(void)
{
	/*
	 * Load trap vectors. All traps run on a dedicated trap stack, except
	 * critical and debug exceptions, which have a separate stack.
	 *
	 * NOTE: Nested IRQs/exceptions must not use IST as the CPU switches
	 * to the configured stack pointer irrespective of the fact that it may
	 * already run on the same stack. For example, if we would cause a
	 * page fault in the debug trap and both would use the same IST entry,
	 * the page fault handler corrupts the stack (of the debug trap) and
	 * the system eventually crashes when returning from the page fault
	 * handler.
	 */
#define FILL_TRAP_GATE(name, ist)					\
	extern void cpu_trap_##name(void);				\
	idt_fillgate(TRAP_##name, ASM_TRAP_SYM(name), ist)

	FILL_TRAP_GATE(divide_error,		2);
	FILL_TRAP_GATE(debug,			3); /* runs on IST3 (cpu_crit_stack) */

	FILL_TRAP_GATE(overflow,		2);
	FILL_TRAP_GATE(bounds,			2);
	FILL_TRAP_GATE(invalid_op,		2);
	FILL_TRAP_GATE(no_device,		2);

	FILL_TRAP_GATE(coproc_seg_overrun,	2);
	FILL_TRAP_GATE(invalid_tss,		2);
	FILL_TRAP_GATE(no_segment,		2);
	FILL_TRAP_GATE(stack_error,		2);
	FILL_TRAP_GATE(gp_fault,		2);
	FILL_TRAP_GATE(page_fault,		2);
	FILL_TRAP_GATE(spurious_int,		2);

	FILL_TRAP_GATE(coproc_error,		2);
	FILL_TRAP_GATE(alignment_check,		2);
	FILL_TRAP_GATE(simd_error,		2);

	/* xen hypervisor callback */
	FILL_TRAP_GATE(hypervisor_callback,	2);

	idtptr.limit = sizeof(cpu_idt) - 1;
	idtptr.base = (__u64) &cpu_idt;
}

void traps_lcpu_init(struct lcpu *this_lcpu)
{
	int rc;

	gdt_init(this_lcpu->idx);
	tss_init(this_lcpu->idx);
	idt_init();

	/* setup xen hypervisor callback */
	rc = hvm_set_parameter(HVM_PARAM_CALLBACK_IRQ, (2ULL << 56) | TRAP_hypervisor_callback);
	UK_ASSERT(rc == 0);
}
#endif
