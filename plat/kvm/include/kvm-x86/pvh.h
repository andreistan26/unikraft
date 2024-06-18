/* SPDX-License-Identifier: MIT AND BSD-3-Clause */
/* Copyright (c) 2022, Unikraft GmbH and The Unikraft Authors.
 * Copyright (c) 2016, Citrix Systems, Inc.
 * Licensed under the BSD-3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 */

#ifndef __PVH_H__
#define __PVH_H__

#define PVH_START_MAGIC_VALUE	0x336ec578

#define PVH_MEMMAP_TYPE_RAM		1
#define PVH_MEMMAP_TYPE_RESERVED	2
#define PVH_MEMMAP_TYPE_ACPI		3
#define PVH_MEMMAP_TYPE_NVS		4
#define PVH_MEMMAP_TYPE_UNUSABLE	5
#define PVH_MEMMAP_TYPE_DISABLED	6
#define PVH_MEMMAP_TYPE_PMEM		7

#ifndef __ASSEMBLY__
#include <uk/essentials.h>

/*
 * C representation of the x86/HVM start info layout.
 */
struct pvh_start_info {
	__u32 magic;		/* Contains the magic value 0x336ec578       */
				/* ("xEn3" with the 0x80 bit of the "E" set).*/
	__u32 version;	/* Version of this structure.                */
	__u32 flags;		/* SIF_xxx flags.                            */
	__u32 nr_modules;	/* Number of modules passed to the kernel.   */
	__u64 modlist_paddr; /* Physical address of an array of           */
				/* hvm_modlist_entry.                        */
	__u64 cmdline_paddr; /* Physical address of the command line.     */
	__u64 rsdp_paddr;	/* Physical address of the RSDP ACPI data    */
				/* structure.                                */
	/* All following fields only present in version 1 and newer */
	__u64 memmap_paddr;	 /* Physical address of an array of           */
				 /* hvm_memmap_table_entry.                   */
	__u32 memmap_entries; /* Number of entries in the memmap table.    */
				 /* Value will be zero if there is no memory  */
				 /* map being provided.                       */
	__u32 reserved;	 /* Must be zero.                             */
};

struct pvh_modlist_entry {
	__u64 paddr;             /* Physical address of the module.           */
	__u64 size;              /* Size of the module in bytes.              */
	__u64 cmdline_paddr;     /* Physical address of the command line.     */
	__u64 reserved;
};

struct pvh_memmap_table_entry {
	__u64 addr;              /* Base address of the memory region         */
	__u64 size;              /* Size of the memory region in bytes        */
	__u32 type;              /* Mapping type                              */
	__u32 reserved;          /* Must be zero for Version 1.               */
};

#endif /* !__ASSEMBLY__ */
#endif /* ! __PVH_H__ */
