#include <xen-x86/pvh.h>
#include <uk/essentials.h>
#include <uk/plat/common/lcpu.h>
#include <uk/plat/common/bootinfo.h>
#include <uk/plat/bootstrap.h>
#include <uk/reloc.h>

#include <stdint.h>
#include <x86/cpu.h>
#include <xen/xen.h>
#include <xen/arch-x86/cpuid.h>
#include <xen-x86/hypercall64.h>
#include <x86/traps.h>

#define pvh_crash(msg, rc)	ukplat_crash()

shared_info_t *HYPERVISOR_shared_info;
void _ukplat_entry(struct lcpu *lcpu, struct ukplat_bootinfo *bi);

/* Initialize hypercalls */
static void hpc_init(void)
{
    uint32_t eax, ebx, ecx, edx, base;

    for ( base = XEN_CPUID_FIRST_LEAF;
          base < XEN_CPUID_FIRST_LEAF + 0x10000; base += 0x100 )
    {
        cpuid(base, 0, &eax, &ebx, &ecx, &edx);

        if ( (ebx == XEN_CPUID_SIGNATURE_EBX) &&
             (ecx == XEN_CPUID_SIGNATURE_ECX) &&
             (edx == XEN_CPUID_SIGNATURE_EDX) &&
             ((eax - base) >= 2) )
            break;
    }

    cpuid(base + 2, 0, &eax, &ebx, &ecx, &edx);
    wrmsrl(ebx, (unsigned long)&hypercall_page);
    barrier();
}

static void pvh_init_cmdline(struct ukplat_bootinfo *bi,
			     struct pvh_start_info *pi)
{
	struct ukplat_memregion_desc mrd = {0};
	int rc;

	bi->cmdline = pi->cmdline_paddr;
	if (unlikely(pi->cmdline_paddr == NULL))
		return;

	bi->cmdline_len = strlen((char *) pi->cmdline_paddr);
	if (bi->cmdline_len == 0)
		return;

	mrd.type = UKPLAT_MEMRT_RESERVED;
	mrd.flags = UKPLAT_MEMRF_READ;
	mrd.pbase = PAGE_ALIGN_DOWN(pi->cmdline_paddr);
	mrd.vbase = mrd.pbase;
	mrd.len = PAGE_ALIGN_UP(bi->cmdline_len);
	mrd.pg_count = PAGE_COUNT(mrd.pg_off + mrd.len);

	rc = ukplat_memregion_list_insert(&bi->mrds, &mrd);
	if (unlikely(rc < 0))
		pvh_crash("Unable to add cmdline mapping", rc);
}

static void pvh_init_initrd(struct ukplat_bootinfo *bi,
			    struct pvh_start_info *pi)
{
	struct ukplat_memregion_desc mrd = {0};
	struct pvh_modlist_entry *mod, *table;
	__u32 i;
	int rc;

	table = (struct pvh_modlist_entry *)pi->modlist_paddr;
	for (i = 0; i < pi->nr_modules; i++) {
		mod = &table[i];

		mrd.type = UKPLAT_MEMRT_INITRD;
		mrd.flags = UKPLAT_MEMRF_READ;
		mrd.pbase = PAGE_ALIGN_DOWN(mod->paddr);
		mrd.vbase = mrd.pbase;
		mrd.len = mod->size;
		mrd.pg_count = PAGE_COUNT(mrd.pg_off + mrd.len);
#ifdef CONFIG_UKPLAT_MEMRNAME
		memcpy(mrd.name, "initrd", sizeof("initrd"));
#endif /* CONFIG_UKPLAT_MEMRNAME */

		rc = ukplat_memregion_list_insert(&bi->mrds, &mrd);
		if (unlikely(rc < 0))
			pvh_crash("Unable to add initrd mapping", rc);
	}
}

static void pvh_init_memory(struct ukplat_bootinfo *bi,
			    struct pvh_start_info *pi)
{
	struct pvh_memmap_table_entry *entry, *table;
	struct ukplat_memregion_desc mrd = {0};
	__u64 start, end;
	__u32 i;
	int rc;

	/* FIXME: legacy himem? */

	if (unlikely(pi->version < 1))
		return;

	table = (struct pvh_memmap_table_entry *)pi->memmap_paddr;
	for (i = 0; i < pi->memmap_entries; i++) {
		entry = &table[i];

		/* Kludge: Don't add zero-page, because the platform code does
		 * not handle address zero well.
		 */
		start = MAX(entry->addr, __PAGE_SIZE);
		end = entry->addr + entry->size;
		if (end <= start)
			continue;

		mrd.pbase = PAGE_ALIGN_DOWN(start);
		mrd.vbase = mrd.pbase;
		mrd.len = end - start;
		mrd.pg_off = start - mrd.pbase;
		mrd.pg_count = PAGE_COUNT(mrd.pg_off + mrd.len);

		if (entry->type == PVH_MEMMAP_TYPE_RAM) {
			mrd.type = UKPLAT_MEMRT_FREE;
			mrd.flags = UKPLAT_MEMRF_READ | UKPLAT_MEMRF_WRITE;
		} else {
			mrd.type = UKPLAT_MEMRT_RESERVED;
			mrd.flags = UKPLAT_MEMRF_READ;
		}

		rc = ukplat_memregion_list_insert(&bi->mrds, &mrd);
		if (unlikely(rc < 0))
			pvh_crash("Unable to add ram mapping", rc);
	}
}

void libxenplat_start(struct lcpu *lcpu, struct pvh_start_info *pi)
{
	struct ukplat_bootinfo *bi;

	/* Initialize hypercall page */
	hpc_init();

	/* Initialize traps */
	traps_table_init();

	HYPERVISOR_shared_info = map_shared_info(pi);

	/* Setup events */
	init_events();

	bi = ukplat_bootinfo_get();
	if (unlikely(!bi))
		pvh_crash("Incompatible or corrupted bootinfo", -EINVAL);

	if (pi->magic != PVH_START_MAGIC_VALUE)
		pvh_crash("Invalid magic number", -EINVAL);

	/* We have to call this here as the very early do_uk_reloc32 relocator
	 * does not also relocate the UKPLAT_MEMRT_KERNEL mrd's like its C
	 * equivalent, do_uk_reloc, does.
	 */
	do_uk_reloc_kmrds(0, 0);

	pvh_init_cmdline(bi, pi);
	pvh_init_initrd(bi, pi);
	pvh_init_memory(bi, pi);
	ukplat_memregion_list_coalesce(&bi->mrds);

	memcpy(bi->bootprotocol, "pvh", sizeof("pvh"));

	_ukplat_entry(lcpu, bi);
}
