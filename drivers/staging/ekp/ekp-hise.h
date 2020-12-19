/*
 * drivers/staging/ekp/ekp-hise.h
 *
 * Copyright (C) 2018 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _EKP_HISE_H
#define _EKP_HISE_H

#ifdef __ASSEMBLY__

#ifdef CONFIG_64BIT
	.macro	data, val
	.quad	\val
	.endm
#else
	.macro	data, val
	.long	\val
	.endm
#endif

#if EKP_USE_FW
#ifdef CONFIG_ARM64
#define EKP_ALIGN	\
	.balign	2048
#else
#define EKP_ALIGN	\
	.balign	32
#endif
#define EKP_HISE_STUB	\
	.incbin	CONFIG_EKP_HISE_FIRMWARE, 0, 2048
#define EKP_HISE	\
	.incbin	CONFIG_EKP_HISE_FIRMWARE, 2048
#else	/* !EKP_USE_FW */
#define EKP_ALIGN	\
	.balign	8
#define EKP_HISE_STUB
#define EKP_HISE
#endif	/* EKP_USE_FW */

#endif /* __ASSEMBLY__ */

/*
 * Error codes from EKP HISE F/W
 * These values should be the same as the values in error.h for HISE F/W
 */
#define EKP_HISE_OK			0
#define EKP_HISE_CMD_NOT_SUPPORT	1
#define EKP_HISE_DETECT_VIOLATION	2
#define 	INIT_AFTER_START	(1 << 0)
#define 	SET_PUD_VIOLATION	(1 << 1)
#define 	SET_PMD_VIOLATION	(1 << 2)
#define 	SET_PTE_VIOLATION	(1 << 3)
#define 	SET_RO_S2_VIOLATION	(1 << 4)
#define 	SLAB_SET_FP_VIOLATION	(1 << 5)
#define 	CRED_INIT_VIOLATION	(1 << 6)
#define 	CRED_SECURITY_VIOLATION	(1 << 7)
#define 	SET_MOD_SECT_S2_VIOLATION	(1 << 8)
#define 	UNSET_MOD_CORE_S2_VIOLATION	(1 << 9)
#define 	UNSET_MOD_INIT_S2_VIOLATION	(1 << 10)
#define 	SET_ROMEMPOOL_VIOLATION	(1 << 11)
#define 	UNKNOWN_VIOLATION	(1 << 12)

#define	EKP_HISE_VIOLATION_MAX	13

#endif /* _EKP_HISE_H */
