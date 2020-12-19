/*
 * Copyright (c) 2018 Jias Huang <jias_huang@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>

#include "vrpc.h"

static int __init vutils_drv_init(void)
{
	vrpc_init();
	return 0;
}

static void __exit vutils_drv_exit(void)
{
	vrpc_exit();
}

module_init(vutils_drv_init);
module_exit(vutils_drv_exit);
