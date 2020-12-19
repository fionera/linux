/*
 * drivers/staging/ekp/ekp-hise.S
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

#include <linux/init.h>
#include "ekp-hise.h"

	__INITRODATA

	.global	__ekp_hise_header

	EKP_ALIGN
__ekp_hise_stub:
	EKP_HISE_STUB
__ekp_hise_header:
	EKP_HISE
__ekp_hise_end:

	.global	ekp_hise_stub
ekp_hise_stub:
	data	__ekp_hise_stub

	.global ekp_hise_size
ekp_hise_size:
	data	__ekp_hise_end-__ekp_hise_stub

	.end
