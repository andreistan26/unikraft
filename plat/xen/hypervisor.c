/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * hypervisor.c
 *
 * Communication to/from hypervisor.
 *
 * Copyright (c) 2002-2003, K A Fraser
 * Copyright (c) 2005, Grzegorz Milos, gm281@cam.ac.uk,Intel Research Cambridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdint.h>

#include <common/hypervisor.h>
#include <common/events.h>
#if (defined __X86_32__) || (defined __X86_64__)
#include <xen-x86/smp.h>
#elif (defined __ARM_32__) || (defined __ARM_64__)
#include <xen-arm/smp.h>
#endif

#include <xen-x86/mm.h>
#include <xen/memory.h>
#include <xen/hvm/hvm_op.h>
#include <uk/arch/lcpu.h>
#include <uk/atomic.h>
#include <uk/bitops.h>

#define active_evtchns(sh, idx)				\
	((sh)->evtchn_pending[idx] & ~(sh)->evtchn_mask[idx])

int in_callback;

#if ! defined(XEN_PARAVIRT)
/* Get shared info from Xen */
shared_info_t *map_shared_info()
{
    struct xen_add_to_physmap xatp;
    extern shared_info_t _libxenplat_shared_info;

    xatp.domid = DOMID_SELF;
    xatp.idx = 0;
    xatp.space = XENMAPSPACE_shared_info;
    xatp.gpfn = PFN_DOWN((size_t)&_libxenplat_shared_info);
    if ( HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp) != 0 )
        return NULL;

    return &_libxenplat_shared_info;
}
#endif

int hvm_get_parameter(int idx, uint64_t *value)
{
    struct xen_hvm_param xhv;
    int ret;

    xhv.domid = DOMID_SELF;
    xhv.index = idx;
    ret = HYPERVISOR_hvm_op(HVMOP_get_param, &xhv);
    UK_ASSERT(ret >= 0);

    *value = xhv.value;

    return ret;
}

int hvm_set_parameter(int idx, uint64_t value)
{
    struct xen_hvm_param xhv;

    xhv.domid = DOMID_SELF;
    xhv.index = idx;
    xhv.value = value;

    return HYPERVISOR_hvm_op(HVMOP_set_param, &xhv);
}


void do_hypervisor_callback(struct __regs *regs)
{
	unsigned long l1, l2, l1i, l2i;
	unsigned int port;
	int cpu = 0;
	shared_info_t *s = HYPERVISOR_shared_info;
	vcpu_info_t *vcpu_info = &s->vcpu_info[cpu];

	in_callback = 1;

	vcpu_info->evtchn_upcall_pending = 0;
/* NB x86. No need for a barrier here -- XCHG is a barrier on x86. */
#if !(defined __X86_32__ || defined __X86_64__)
	/* Clear master flag /before/ clearing selector flag. */
	wmb();
#endif
	l1 = uk_exchange_n(&vcpu_info->evtchn_pending_sel, 0);
	while (l1 != 0) {
		l1i = uk_ffsl(l1);
		l1 &= ~(1UL << l1i);

		while ((l2 = active_evtchns(s, l1i)) != 0) {
			l2i = uk_ffsl(l2);
			l2 &= ~(1UL << l2i);

			port = (l1i * (sizeof(unsigned long) * 8)) + l2i;
			do_event(port, regs);
		}
	}

	in_callback = 0;
}

void ukplat_lcpu_irqs_handle_pending(void)
{
#ifdef XEN_HAVE_PV_UPCALL_MASK
	int save;
#endif
	vcpu_info_t *vcpu;

	vcpu = &HYPERVISOR_shared_info->vcpu_info[smp_processor_id()];
#ifdef XEN_HAVE_PV_UPCALL_MASK
	save = vcpu->evtchn_upcall_mask;
#endif

	while (vcpu->evtchn_upcall_pending) {
#ifdef XEN_HAVE_PV_UPCALL_MASK
		vcpu->evtchn_upcall_mask = 1;
#endif
		barrier();
		do_hypervisor_callback(NULL);
		barrier();
#ifdef XEN_HAVE_PV_UPCALL_MASK
		vcpu->evtchn_upcall_mask = save;
		barrier();
#endif
	};
}
