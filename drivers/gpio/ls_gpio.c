// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 TangHaifeng <tang-haifeng@foxmail.com>
 *
 * Based on the driver mt7621_gpio.c
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <malloc.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dm/device-internal.h>
#include <dt-bindings/gpio/gpio.h>

#define LS_MAX_BANK		4
#define LS_BANK_WIDTH		32

enum ls_gpio_reg {
#if defined(CONFIG_SOC_LS2K1000)
	GPIO_DIR = 0,
	GPIO_OUTPUT,
	GPIO_DATA,
#elif defined(CONFIG_SOC_LS2K500)
	GPIO_DIR = 0,
	GPIO_DATA,
	GPIO_OUTPUT,
#elif defined(CONFIG_SOC_LS3A)
	GPIO_DIR = 0,
	GPIO_CFG,
	GPIO_DATA,
	GPIO_OUTPUT,
#endif
};

struct ls_gpio_platdata {
	void __iomem *base;
	char bank_name[3];	/* Name of bank, e.g. "PA", "PB" etc */
	int gpio_count;
	int bank;
};

static u32 reg_offs(struct ls_gpio_platdata *plat, int reg)
{
#if defined(CONFIG_SOC_LS2K500)
	return (reg * 0x08) + (plat->bank * 0x4);
#elif defined(CONFIG_SOC_LS2K1000)
	return (reg * 0x10) + (plat->bank * 0x4);
#elif defined(CONFIG_SOC_LS3A)
	return (reg * 0x4);
#endif
}

static int ls_gpio_get_value(struct udevice *dev, unsigned int offset)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);

	return !!(ioread32(plat->base +
			   reg_offs(plat, GPIO_DATA)) & BIT(offset));
}

static int ls_gpio_set_value(struct udevice *dev, unsigned int offset,
				   int value)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);

	if (value)
		setbits_le32(plat->base + reg_offs(plat, GPIO_OUTPUT),
		     BIT(offset));
	else
		clrbits_le32(plat->base + reg_offs(plat, GPIO_OUTPUT),
		     BIT(offset));

	return 0;
}

static int ls_gpio_direction_input(struct udevice *dev, unsigned int offset)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);

	setbits_le32(plat->base + reg_offs(plat, GPIO_DIR),
		     BIT(offset));

	return 0;
}

static int ls_gpio_direction_output(struct udevice *dev, unsigned int offset,
					  int value)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);

	clrbits_le32(plat->base + reg_offs(plat, GPIO_DIR),
		     BIT(offset));
	ls_gpio_set_value(dev, offset, value);

	return 0;
}

static int ls_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);
	u32 t;

	t = ioread32(plat->base + reg_offs(plat, GPIO_DIR));
	if (t & BIT(offset))
		return GPIOF_INPUT;

	return GPIOF_OUTPUT;
}

static int ls_gpio_request(struct udevice *dev, unsigned offset,
			     const char *label)
{
#if defined(CONFIG_SOC_LS3A)
	struct ls_gpio_platdata *plat = dev_get_plat(dev);
	// active low.
	clrbits_le32(plat->base + reg_offs(plat, GPIO_CFG),
		     BIT(offset));
#endif
	return 0;
}

static int ls_gpio_free(struct udevice *dev, unsigned offset)
{
#if defined(CONFIG_SOC_LS3A)
	struct ls_gpio_platdata *plat = dev_get_plat(dev);

	setbits_le32(plat->base + reg_offs(plat, GPIO_CFG),
		     BIT(offset));
#endif
	return 0;
}

static const struct dm_gpio_ops gpio_ls_ops = {
	.request 			= ls_gpio_request,
	.rfree 				= ls_gpio_free,
	.direction_input	= ls_gpio_direction_input,
	.direction_output	= ls_gpio_direction_output,
	.get_value			= ls_gpio_get_value,
	.set_value			= ls_gpio_set_value,
	.get_function		= ls_gpio_get_function,
};

static int ls_gpio_probe(struct udevice *dev)
{
	struct ls_gpio_platdata *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	/* Tell the uclass how many GPIOs we have */
	if (plat) {
		uc_priv->gpio_count = plat->gpio_count;
		uc_priv->bank_name = plat->bank_name;
	}

	return 0;
}

/**
 * We have a top-level GPIO device with no actual GPIOs. It has a child
 * device for each Loongson1 bank.
 */
static int ls_gpio_bind(struct udevice *parent)
{
	struct ls_gpio_platdata *plat = parent->plat_;
	void __iomem *membase;
	ofnode node;
	int bank = 0;
	int ret;

	/* If this is a child device, there is nothing to do here */
	if (plat)
		return 0;

	membase = dev_remap_addr(parent);
	if (!membase)
		return -EINVAL;

	for (node = dev_read_first_subnode(parent); ofnode_valid(node);
	     node = dev_read_next_subnode(node)) {
		struct ls_gpio_platdata *plat;
		struct udevice *dev;

		plat = calloc(1, sizeof(*plat));
		if (!plat)
			return -ENOMEM;
		plat->base = membase;
		plat->bank_name[0] = 'P';
		plat->bank_name[1] = 'A' + bank;
		plat->bank_name[2] = '\0';
		plat->gpio_count = LS_BANK_WIDTH;
		plat->bank = bank;

		ret = device_bind(parent, parent->driver,
				  plat->bank_name, plat, node, &dev);
		if (ret)
			return ret;

		bank++;
	}

	return 0;
}

static const struct udevice_id ls_gpio_ids[] = {
	{ .compatible = "loongson,ls-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_ls) = {
	.name	= "gpio_ls",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_ls_ops,
	.of_match = ls_gpio_ids,
	.bind	= ls_gpio_bind,
	.probe	= ls_gpio_probe,
};
