// SPDX-License-Identifier: GPL-2.0+
/*
 * Sitronix ST7701/ST7701S RGB panel bootloader helper for U-Boot 2018.01.
 *
 * Uses the panel init sequence from the DT node under spi1 and performs
 * a bootloader handoff by setting sitronix,bootloader-initialized.
 */

#define LOG_CATEGORY UCLASS_PANEL

#include <common.h>
#include <dm.h>
#include <dm/read.h>
#include <libfdt.h>
#include <log.h>
#include <panel.h>
#include <spi.h>
#include <asm/gpio.h>

#define ST7701_SEQ_CMD      0x01
#define ST7701_SEQ_DELAY    0x02
#define ST7701_SEQ_MAX_DATA 64
#define MIPI_DCS_EXIT_SLEEP_MODE 0x11
#define MIPI_DCS_SET_DISPLAY_ON  0x29

struct st7701_rgb_priv {
	struct spi_slave *slave;
	struct gpio_desc reset;
	struct gpio_desc enable;
	struct gpio_desc dc;
	bool has_reset;
	bool has_enable;
	bool has_dc;
	bool bootloader_initialized;
};

static bool st7701_rgb_bootloader_initialized;

static inline bool st7701_rgb_gpio_active(struct gpio_desc *desc, bool valid)
{
	return valid && dm_gpio_is_valid(desc);
}

static int st7701_rgb_spi_write_byte(struct st7701_rgb_priv *priv,
				     bool is_data, u8 value,
				     unsigned long flags)
{
	u16 tx;

	if (priv->has_dc) {
		dm_gpio_set_value(&priv->dc, is_data);
		return spi_xfer(priv->slave, 8, &value, NULL, flags);
	}

	tx = ((is_data ? 1 : 0) << 8) | value;
	return spi_xfer(priv->slave, 9, &tx, NULL, flags);
}

static int st7701_rgb_write_cmd(struct st7701_rgb_priv *priv, u8 cmd,
				const u8 *data, u8 len)
{
	unsigned long flags;
	int ret;
	u8 i;

	flags = SPI_XFER_BEGIN;
	if (!len)
		flags |= SPI_XFER_END;

	ret = st7701_rgb_spi_write_byte(priv, false, cmd, flags);
	if (ret)
		return ret;

	for (i = 0; i < len; i++) {
		flags = 0;
		if (i == len - 1)
			flags |= SPI_XFER_END;

		ret = st7701_rgb_spi_write_byte(priv, true, data[i], flags);
		if (ret)
			return ret;
	}

	return 0;
}

static int st7701_rgb_run_init_sequence(struct udevice *dev,
					struct st7701_rgb_priv *priv)
{
	const u8 *seq;
	int len;
	int pos = 0;
	int ret;

	seq = fdt_getprop(gd->fdt_blob, dev_of_offset(dev),
			  "sitronix,init-sequence", &len);
	if (!seq || !len)
		return -EINVAL;

	while (pos < len) {
		u8 op, arg, n;

		if (len - pos < 3)
			return -EINVAL;

		op = seq[pos++];
		arg = seq[pos++];
		n = seq[pos++];

		switch (op) {
		case ST7701_SEQ_CMD:
			if (n > ST7701_SEQ_MAX_DATA || len - pos < n)
				return -EINVAL;

			ret = st7701_rgb_write_cmd(priv, arg, seq + pos, n);
			if (ret)
				return ret;

			pos += n;
			break;
		case ST7701_SEQ_DELAY:
			if (n)
				return -EINVAL;
			mdelay(arg);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static void st7701_rgb_reset(struct st7701_rgb_priv *priv)
{
	if (!st7701_rgb_gpio_active(&priv->reset, priv->has_reset))
		return;

	dm_gpio_set_value(&priv->reset, 1);
	mdelay(20);
	dm_gpio_set_value(&priv->reset, 0);
	mdelay(120);
}

static int st7701_rgb_init_panel(struct udevice *dev)
{
	struct st7701_rgb_priv *priv = dev_get_priv(dev);
	int busnum;
	u32 cs;
	u32 speed;
	int ret;

	if (priv->bootloader_initialized)
		return 0;

	if (!dev->parent)
		return -ENODEV;

	ret = dev_read_alias_seq(dev->parent, &busnum);
	if (ret)
		busnum = 1;

	cs = dev_read_u32_default(dev, "reg", 0);
	speed = dev_read_u32_default(dev, "spi-max-frequency", 1000000);
	priv->slave = spi_setup_slave(busnum, cs, speed, SPI_MODE_0);
	if (!priv->slave)
		return -ENODEV;

	ret = gpio_request_by_name(dev, "dc-gpios", 0, &priv->dc, GPIOD_IS_OUT);
	if (ret && ret != -ENOENT)
		goto err_free_slave;
	priv->has_dc = !ret;

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset,
				   GPIOD_IS_OUT);
	if (ret && ret != -ENOENT)
		goto err_free_slave;
	priv->has_reset = !ret;

	ret = gpio_request_by_name(dev, "enable-gpios", 0, &priv->enable,
				   GPIOD_IS_OUT);
	if (ret && ret != -ENOENT)
		goto err_free_slave;
	priv->has_enable = !ret;

	if (priv->has_enable)
		dm_gpio_set_value(&priv->enable, 1);

	ret = spi_claim_bus(priv->slave);
	if (ret)
		goto err_free_slave;

	if (!priv->has_dc)
		priv->slave->wordlen = 9;

	st7701_rgb_reset(priv);

	ret = st7701_rgb_run_init_sequence(dev, priv);
	if (ret)
		goto err_release_bus;

	ret = st7701_rgb_write_cmd(priv, MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (ret)
		goto err_release_bus;

	mdelay(120);

	ret = st7701_rgb_write_cmd(priv, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (ret)
		goto err_release_bus;

	mdelay(20);

	spi_release_bus(priv->slave);
	priv->bootloader_initialized = true;
	st7701_rgb_bootloader_initialized = true;

	return 0;

err_release_bus:
	spi_release_bus(priv->slave);
err_free_slave:
	spi_free_slave(priv->slave);
	priv->slave = NULL;
	return ret;
}

int st7701_rgb_fixup_fdt(void *blob)
{
	int node;

	if (!st7701_rgb_bootloader_initialized)
		return 0;

	node = fdt_node_offset_by_compatible(blob, -1, "sitronix,st7701-rgb-spi");
	if (node < 0)
		node = fdt_node_offset_by_compatible(blob, -1,
					     "sitronix,st7701s-rgb-spi");
	if (node < 0)
		return node;

	return fdt_setprop(blob, node, "sitronix,bootloader-initialized", "", 1);
}

int st7701_rgb_bootloader_init(void *blob)
{
	struct udevice *dev;
	int node;
	int ret;

	node = fdt_node_offset_by_compatible(blob, -1, "sitronix,st7701-rgb-spi");
	if (node < 0)
		node = fdt_node_offset_by_compatible(blob, -1,
					     "sitronix,st7701s-rgb-spi");
	if (node < 0)
		return node;

	ret = uclass_get_device_by_of_offset(UCLASS_PANEL, node, &dev);
	if (ret)
		return ret;

	return 0;
}

int st7701_rgb_bootloader_setup(void *blob)
{
	int ret;

	ret = st7701_rgb_bootloader_init(blob);
	if (ret)
		return ret;

	return st7701_rgb_fixup_fdt(blob);
}

static int st7701_rgb_enable_backlight(struct udevice *dev)
{
	return 0;
}

static int st7701_rgb_probe(struct udevice *dev)
{
	return st7701_rgb_init_panel(dev);
}

static const struct panel_ops st7701_rgb_ops = {
	.enable_backlight = st7701_rgb_enable_backlight,
};

static const struct udevice_id st7701_rgb_ids[] = {
	{ .compatible = "sitronix,st7701-rgb-spi" },
	{ .compatible = "sitronix,st7701s-rgb-spi" },
	{ }
};

U_BOOT_DRIVER(st7701_rgb_panel) = {
	.name = "st7701_rgb_panel",
	.id = UCLASS_PANEL,
	.of_match = st7701_rgb_ids,
	.ops = &st7701_rgb_ops,
	.probe = st7701_rgb_probe,
	.priv_auto_alloc_size = sizeof(struct st7701_rgb_priv),
};
