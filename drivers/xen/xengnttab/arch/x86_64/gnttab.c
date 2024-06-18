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

#include <stdint.h>
#include <stddef.h>
#include <uk/print.h>
#include <xen/xen.h>
#include <xen/grant_table.h>
#include <common/hypervisor.h>

#if defined(XEN_PARAVIRT)
#include <xen-x86/mm.h>
#else
#include <uk/plat/paging.h>
#include <uk/plat/memory.h>
#include <uk/plat/common/memory.h>
#include <xen/memory.h>
#endif

#if defined(XEN_PARAVIRT)
grant_entry_v1_t *gnttab_arch_init(int grant_frames_num)
{
	grant_entry_v1_t *gnte = NULL;
	struct gnttab_setup_table setup;
	unsigned long frames[grant_frames_num];
	int rc;

	setup.dom = DOMID_SELF;
	setup.nr_frames = grant_frames_num;
	set_xen_guest_handle(setup.frame_list, frames);

	rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
	if (rc) {
		uk_pr_err("Hypercall error: %d\n", rc);
		goto out;
	}
	if (setup.status != GNTST_okay) {
		uk_pr_err("Hypercall status: %d\n", setup.status);
		goto out;
	}

	gnte = map_frames(frames, grant_frames_num, ukplat_memallocator_get());

out:
	return gnte;
}
#else

/* Find memory location where `pages` pages can be added to
 * an unallocated space */
void *gnttab_reserve_memory(unsigned int pages)
{
	struct ukplat_memregion_desc *mrd;
	__sz previous_region_end = 0;
	__sz size = (unsigned long) pages * PAGE_SIZE;

	ukplat_memregion_foreach(&mrd, 0, 0, 0) {
		if (previous_region_end == 0) {
			previous_region_end = mrd->pbase + mrd->pg_count * PAGE_SIZE;
			continue;
		}

		if (previous_region_end + size <= mrd->pbase)
			return (void *) previous_region_end;

		previous_region_end = mrd->pbase + mrd->len;
	}
	/* The address must be a valid 32bit address as we are unsing gnttab v1
	 * entries */

	return (void *)previous_region_end;
}

grant_entry_v1_t *gnttab_arch_init(int grant_frames_num)
{
	int i, rc;
	__sz pfn;
	struct xen_add_to_physmap xatp;
	struct uk_pagetable *pt;
	grant_entry_v1_t *gnte = NULL;

	pt = ukplat_pt_get_active();
	gnte = (grant_entry_v1_t *)gnttab_reserve_memory(grant_frames_num);
	pfn = (__sz)gnte >> PAGE_SHIFT;

	for (i = grant_frames_num - 1; i >= 0; i--) {
		xatp.domid = DOMID_SELF;
		xatp.idx = i;
		xatp.space = XENMAPSPACE_grant_table;
		xatp.gpfn = pfn + i;
		rc = HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp);
		if (rc < 0) {
		    uk_pr_err("Could not init grant table\n");
		    return NULL;
		}
	}
	rc = ukplat_page_map(pt, (__sz) gnte, (__sz) gnte, grant_frames_num, PAGE_ATTR_PROT_RW, 0);
	if (rc < 0) {
		uk_pr_err("Could not map grant pages at address %p, err=%d\n", gnte, rc);
		return NULL;
	}

	return gnte;
}
#endif
