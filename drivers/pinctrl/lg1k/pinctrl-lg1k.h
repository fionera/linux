#ifndef __PINCTRL_LG1K_H__
#define __PINCTRL_LG1K_H__

#include <linux/types.h>

struct pinmux_reg {
	phys_addr_t const pa;	/* physical address to a pinmux register */
	u32 const mask;		/* bit-mask NOT to be restored at resume */
	void __iomem *va;	/* virtual address for pa */
	u32 save;		/* saved register value at suspend */
};

struct pinmux_desc {
	struct pinmux_reg const *reg;
	u32 const clr;		/* bit-mask to clear */
	u32 const set;		/* bit-mask to set */
};

struct gpio_pinmux_desc {
	unsigned int const gpio;
	unsigned int const ngpios;
	struct pinmux_desc const pinmux_desc;
};

#define GPIO_IN_DESC_RANGE(_gpio,_desc)	\
	((_gpio) >= (_desc)->gpio && (_gpio) < (_desc)->gpio + (_desc)->ngpios)

struct group_desc {
	char const *name;
	unsigned int const *pins;
	unsigned int const num_pins;
	struct pinmux_desc const *pinmux_descs;
	unsigned int const num_pinmux_descs;
};

#define GROUP_DESC(_name)	\
	{								\
		.name = #_name,						\
		.pins = _name ## _pins,					\
		.num_pins = ARRAY_SIZE(_name ## _pins),			\
		.pinmux_descs = _name ## _pinmux_descs,			\
		.num_pinmux_descs = ARRAY_SIZE(_name ## _pinmux_descs),	\
	}

struct function_desc {
	char const *name;
	char const *const *groups;
	size_t const num_groups;
};

#define FUNCTION_DESC(_name)	\
	{							\
		.name = #_name,					\
		.groups = _name ## _groups,			\
		.num_groups = ARRAY_SIZE(_name ## _groups),	\
	}

#endif
