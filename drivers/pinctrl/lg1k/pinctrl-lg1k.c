#include <linux/kernel.h>

#include <linux/io.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>

#include "../core.h"
#include "../pinctrl-utils.h"

#include "pinctrl-lg1k.h"

static int get_groups_count(struct pinctrl_dev *dev)
{
	return ARRAY_SIZE(group_descs);
}

static char const *get_group_name(struct pinctrl_dev *dev, unsigned idx)
{
	return group_descs[idx].name;
}

static int get_group_pins(struct pinctrl_dev *dev, unsigned idx,
                          unsigned const **pins, unsigned *num_pins)
{
	*pins = group_descs[idx].pins;
	*num_pins = group_descs[idx].num_pins;
	return 0;
}

static struct pinctrl_ops pinctrl_ops = {
	.get_groups_count = get_groups_count,
	.get_group_name = get_group_name,
	.get_group_pins = get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_dt_free_map,
};

static int get_functions_count(struct pinctrl_dev *dev)
{
	return ARRAY_SIZE(function_descs);
}

static char const *get_function_name(struct pinctrl_dev *dev, unsigned idx)
{
	return function_descs[idx].name;
}

static int get_function_groups(struct pinctrl_dev *dev, unsigned idx,
                               char const *const **groups, unsigned *num_groups)
{
	*groups = function_descs[idx].groups;
	*num_groups = function_descs[idx].num_groups;
	return 0;
}

static int set_mux(struct pinctrl_dev *dev, unsigned fidx, unsigned gidx)
{
	struct function_desc const *fdesc = &function_descs[fidx];
	struct group_desc const *gdesc = &group_descs[gidx];
	struct pinmux_desc const *pdesc;
	u32 v;
	int i;

	dev_dbg(dev->dev, "set pinmux for %s:%s\n", fdesc->name, gdesc->name);

	for (i = 0; i < gdesc->num_pinmux_descs; i++) {
		pdesc = &gdesc->pinmux_descs[i];
		v = readl_relaxed(pdesc->reg->va);
		v &= ~pdesc->clr;
		v |= pdesc->set;
		writel_relaxed(v, pdesc->reg->va);
	}

	return 0;
}

static int gpio_request_enable(struct pinctrl_dev *dev,
                               struct pinctrl_gpio_range *range,
                               unsigned offset)
{
	struct gpio_pinmux_desc const *desc;
	u32 v;
	int i;

	if (range->pins) {
		for (i = 0; i < range->npins; i++)
			if (offset == range->pins[i])
				break;
	} else {
		for (i = 0; i < range->npins; i++)
			if (offset == range->pin_base + i)
				break;
	}

	if (i == range->npins)
		return 0;

	for (i = 0; i < ARRAY_SIZE(gpio_pinmux_descs); i++) {
		desc = &gpio_pinmux_descs[i];
		if (GPIO_IN_DESC_RANGE(offset, desc)) {
			v = readl_relaxed(desc->pinmux_desc.reg->va);
			v &= ~desc->pinmux_desc.clr;
			v |= desc->pinmux_desc.set;
			writel_relaxed(v, desc->pinmux_desc.reg->va);
		}
	}

	return 0;
}

static void gpio_disable_free(struct pinctrl_dev *dev,
                              struct pinctrl_gpio_range *range, unsigned offset)
{
}

static struct pinmux_ops pinmux_ops = {
	.get_functions_count = get_functions_count,
	.get_function_name = get_function_name,
	.get_function_groups = get_function_groups,
	.set_mux = set_mux,
	.gpio_request_enable = gpio_request_enable,
	.gpio_disable_free = gpio_disable_free,
};

static int pinctrl_suspend(struct device *dev)
{
	struct pinmux_reg *reg;
	int i;

	for (i = 0; i < ARRAY_SIZE(pinmux_regs); i++) {
		reg = &pinmux_regs[i];
		reg->save = readl_relaxed(reg->va);
	}

	return 0;
}

static int pinctrl_resume(struct device *dev)
{
	struct pinmux_reg *reg;
	u32 v;
	int i;

	for (i = 0; i < ARRAY_SIZE(pinmux_regs); i++) {
		reg = &pinmux_regs[i];
		v = readl_relaxed(reg->va);
		v &= reg->mask;
		v |= reg->save & ~reg->mask;
		writel_relaxed(v, reg->va);
	}

	return 0;
}

static struct dev_pm_ops pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pinctrl_suspend, pinctrl_resume)
};

static struct pinctrl_desc pinctrl_desc = {
	.name = MACHINE_NAME "-pinctrl",
	.pins = pin_descs,
	.npins = ARRAY_SIZE(pin_descs),
	.pctlops = &pinctrl_ops,
	.pmxops = &pinmux_ops,
	.owner = THIS_MODULE,
};

static int pinctrl_probe(struct platform_device *pdev)
{
	struct pinctrl_dev *dev;
	struct pinmux_reg *reg;
	int i;

	for (i = 0; i < ARRAY_SIZE(pinmux_regs); i++) {
		reg = &pinmux_regs[i];
		reg->va = ioremap(reg->pa, sizeof(u32));
	}

	dev = pinctrl_register(&pinctrl_desc, &pdev->dev, NULL);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	dev_info(&pdev->dev, MACHINE_NAME " pinctrl driver\n");

	return 0;
}

static struct of_device_id const pinctrl_ids[] = {
	{ .compatible = "lge," MACHINE_NAME "-pinctrl", },
	{ /* sentinel */ }
};

static struct platform_driver pinctrl_driver = {
	.driver = {
		.name = MACHINE_NAME "-pinctrl",
		.of_match_table = pinctrl_ids,
		.pm = &pinctrl_pm_ops,
	},
	.probe = pinctrl_probe,
};

static int __init pinctrl_init(void)
{
	return platform_driver_register(&pinctrl_driver);
}
arch_initcall(pinctrl_init);
