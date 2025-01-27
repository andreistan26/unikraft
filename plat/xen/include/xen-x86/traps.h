/* SPDX-License-Identifier: MIT */
/*
 ****************************************************************************
 * (C) 2005 - Grzegorz Milos - Intel Reseach Cambridge
 ****************************************************************************
 *
 *        File: traps.h
 *      Author: Grzegorz Milos (gm281@cam.ac.uk)
 *
 *        Date: Jun 2005
 *
 * Environment: Xen Minimal OS
 * Description: Deals with traps
 *
 ****************************************************************************
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

#ifndef _TRAPS_H_
#define _TRAPS_H_

#include <x86/traps.h>

#ifndef __ASSEMBLY__
#include <stdint.h>
#include <xen/xen.h>
#endif

#define TRAP_coproc_seg_overrun  9
#define TRAP_spurious_int        15
#define TRAP_xen_callback        32

#ifndef __ASSEMBLY__
/* Assembler stubs */
DECLARE_ASM_TRAP(coproc_seg_overrun);
DECLARE_ASM_TRAP(spurious_int);
DECLARE_ASM_TRAP(hypervisor_callback);
void asm_failsafe_callback(void);
#endif

#define __KERNEL_CS     FLAT_KERNEL_CS
#define __KERNEL_DS     FLAT_KERNEL_DS
#define __KERNEL_SS     FLAT_KERNEL_SS

#ifdef CONFIG_XEN_PVH
/*
 * Global descriptor table (GDT)
 */
#define GDT_DESC_NULL		0
#define GDT_DESC_CODE		1
#define GDT_DESC_DATA		2
#define GDT_DESC_TSS_LO		3
#define GDT_DESC_TSS_HI		4
#define GDT_DESC_TSS		GDT_DESC_TSS_LO

#define GDT_DESC_TYPE_LDT	0x2
#define GDT_DESC_TYPE_TSS_AVAIL	0x9
#define GDT_DESC_TYPE_TSS_BUSY	0xb
#define GDT_DESC_TYPE_CALL	0xc
#define GDT_DESC_TYPE_INTR	0xe
#define GDT_DESC_TYPE_TRAP	0xf

#define GDT_DESC_DPL_KERNEL	0
#define GDT_DESC_DPL_USER	3

#define GDT_DESC_OFFSET(n)	((n) * 0x8)
#define GDT_NUM_ENTRIES		5

/* Seg. Limit                       : 0xfffff
 * Base                             : 0x00000000
 * Type                             : 0xa (execute/read/accessed)
 * Code or Data Segment (S)         : 0x1 (true)
 * Descriptor Privilege Level (DPL) : 0x0 (most privileged)
 * Segment Present (P)              : 0x1 (true)
 * Default Operation Size (D)       : 0x1 (32-bit)
 * Granularity (G)                  : 0x1 (4KiB)
 */
#define GDT_DESC_CODE32_VAL	0x00cf9b000000ffff

/* Seg. Limit                       : 0xfffff
 * Base                             : 0x00000000
 * Type                             : 0x3 (read/write/accessed)
 * Code or Data Segment (S)         : 0x1 (true)
 * Descriptor Privilege Level (DPL) : 0x0 (most privileged)
 * Segment Present (P)              : 0x1 (true)
 * Granularity (G)                  : 0x1 (4KiB)
 */
#define GDT_DESC_DATA32_VAL	0x00cf93000000ffff

/* Seg. Limit                       : 0xfffff
 * Base                             : 0x00000000
 * Type                             : 0xb (execute/read/accessed)
 * Code or Data Segment (S)         : 0x1 (true)
 * Descriptor Privilege Level (DPL) : 0x0 (most privileged)
 * Segment Present (P)              : 0x1 (true)
 * 64-bit Code Segment (L)          : 0x1 (true)
 * Granularity (G)                  : 0x1 (4KiB)
 */
#define GDT_DESC_CODE64_VAL	0x00af9b000000ffff
#define GDT_DESC_DATA64_VAL	GDT_DESC_DATA32_VAL

/*
 * Interrupt descriptor table (LDT)
 */
#define IDT_DESC_CODE		GDT_DESC_CODE

#define IDT_DESC_TYPE_INTR	GDT_DESC_TYPE_INTR

#define IDT_DESC_DPL_KERNEL	GDT_DESC_DPL_KERNEL
#define IDT_DESC_DPL_USER	GDT_DESC_DPL_USER

#define IDT_DESC_OFFSET(n)	GDT_DESC_OFFSET(n)
#define IDT_NUM_ENTRIES		256
#endif

#endif /* _TRAPS_H_ */
