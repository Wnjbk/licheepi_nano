// SPDX-License-Identifier: GPL-2.0+
/*
 * Board-level SiI9022 HDMI transmitter setup for F1C200S.
 *
 * The transmitter control bus is deliberately bit-banged because PA1/PA2
 * are not a F1C200S hardware TWI pin pair. Both lines require external I2C
 * pull-ups. PE12 is sampled as the transmitter's active-high INT/HPD pin.
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>

#define SII9022_I2C_ADDR	0x39

#define SII9022_SDA		SUNXI_GPA(1)
#define SII9022_SCL		SUNXI_GPA(2)
#define SII9022_RESET		SUNXI_GPA(3)
#define SII9022_INT		SUNXI_GPE(12)

#define SII9022_REG_PIXEL_CLK_LSB	0x00
#define SII9022_REG_PIXEL_CLK_MSB	0x01
#define SII9022_REG_VERT_FREQ_LSB	0x02
#define SII9022_REG_VERT_FREQ_MSB	0x03
#define SII9022_REG_TOTAL_PIXELS_LSB	0x04
#define SII9022_REG_TOTAL_PIXELS_MSB	0x05
#define SII9022_REG_TOTAL_LINES_LSB	0x06
#define SII9022_REG_TOTAL_LINES_MSB	0x07
#define SII9022_REG_INPUT_BUS		0x08
#define SII9022_REG_INPUT_FORMAT	0x09
#define SII9022_REG_OUTPUT_FORMAT	0x0a
#define SII9022_REG_SYS_CTRL		0x1a
#define SII9022_REG_POWER		0x1e
#define SII9022_REG_PAGE		0xbc
#define SII9022_REG_OFFSET		0xbd
#define SII9022_REG_ACCESS		0xbe
#define SII9022_REG_TPI_ENABLE		0xc7

#define SII9022_I2C_DELAY_US		5
#define SII9022_SCL_TIMEOUT_US		1000

static u8 sii9022_i2c_addr = SII9022_I2C_ADDR;

/*
 * This U-Boot configuration enables DM_GPIO, but does not instantiate GPIO
 * devices for the suniv PIO banks. Access the proven sunxi PIO registers
 * directly, as the LCD setup code does, instead of using gpio_*().
 */
static struct sunxi_gpio *sii9022_pio(u32 pin)
{
	return BANK_TO_GPIO(GPIO_BANK(pin));
}

static void sii9022_gpio_input(u32 pin)
{
	sunxi_gpio_set_cfgpin(pin, SUNXI_GPIO_INPUT);
}

static void sii9022_gpio_output(u32 pin, int value)
{
	struct sunxi_gpio *pio = sii9022_pio(pin);
	u32 mask = BIT(GPIO_NUM(pin));

	sunxi_gpio_set_cfgpin(pin, SUNXI_GPIO_OUTPUT);
	if (value)
		setbits_le32(&pio->dat, mask);
	else
		clrbits_le32(&pio->dat, mask);
}

static int sii9022_gpio_get(u32 pin)
{
	struct sunxi_gpio *pio = sii9022_pio(pin);

	return !!(readl(&pio->dat) & BIT(GPIO_NUM(pin)));
}

static void sii9022_delay(void)
{
	udelay(SII9022_I2C_DELAY_US);
}

static void sii9022_sda(int high)
{
	if (high)
		sii9022_gpio_input(SII9022_SDA);
	else
		sii9022_gpio_output(SII9022_SDA, 0);
}

static int sii9022_scl(int high)
{
	int timeout;

	if (!high) {
		sii9022_gpio_output(SII9022_SCL, 0);
		return 0;
	}

	sii9022_gpio_input(SII9022_SCL);
	for (timeout = 0; timeout < SII9022_SCL_TIMEOUT_US; timeout++) {
		if (sii9022_gpio_get(SII9022_SCL))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int sii9022_start(void)
{
	int ret;

	sii9022_sda(1);
	ret = sii9022_scl(1);
	if (ret)
		return ret;
	sii9022_delay();
	sii9022_sda(0);
	sii9022_delay();
	return sii9022_scl(0);
}

static void sii9022_stop(void)
{
	sii9022_scl(0);
	sii9022_sda(0);
	sii9022_delay();
	sii9022_scl(1);
	sii9022_delay();
	sii9022_sda(1);
	sii9022_delay();
}

static int sii9022_write_byte(u8 value)
{
	int bit;
	int ret;

	for (bit = 7; bit >= 0; bit--) {
		sii9022_scl(0);
		sii9022_sda(value & BIT(bit));
		sii9022_delay();
		ret = sii9022_scl(1);
		if (ret)
			return ret;
		sii9022_delay();
	}

	sii9022_scl(0);
	sii9022_sda(1);
	sii9022_delay();
	ret = sii9022_scl(1);
	if (ret)
		return ret;
	sii9022_delay();
	ret = sii9022_gpio_get(SII9022_SDA) ? -ENXIO : 0;
	sii9022_scl(0);

	return ret;
}

static int sii9022_read_byte(u8 *value, bool ack)
{
	int bit;
	int ret;
	u8 data = 0;

	sii9022_sda(1);
	for (bit = 7; bit >= 0; bit--) {
		sii9022_scl(0);
		sii9022_delay();
		ret = sii9022_scl(1);
		if (ret)
			return ret;
		sii9022_delay();
		if (sii9022_gpio_get(SII9022_SDA))
			data |= BIT(bit);
	}

	sii9022_scl(0);
	sii9022_sda(!ack);
	sii9022_delay();
	ret = sii9022_scl(1);
	if (!ret)
		sii9022_delay();
	sii9022_scl(0);
	sii9022_sda(1);
	*value = data;

	return ret;
}

static int sii9022_write(u8 reg, u8 value)
{
	int ret;

	ret = sii9022_start();
	if (!ret)
		ret = sii9022_write_byte(sii9022_i2c_addr << 1);
	if (!ret)
		ret = sii9022_write_byte(reg);
	if (!ret)
		ret = sii9022_write_byte(value);
	sii9022_stop();

	return ret;
}

static int sii9022_read(u8 reg, u8 *value)
{
	int ret;

	ret = sii9022_start();
	if (!ret)
		ret = sii9022_write_byte(sii9022_i2c_addr << 1);
	if (!ret)
		ret = sii9022_write_byte(reg);
	if (!ret)
		ret = sii9022_start();
	if (!ret)
		ret = sii9022_write_byte((sii9022_i2c_addr << 1) | 1);
	if (!ret)
		ret = sii9022_read_byte(value, false);
	sii9022_stop();

	return ret;
}

static int sii9022_probe(u8 address)
{
	int ret;

	ret = sii9022_start();
	if (!ret)
		ret = sii9022_write_byte(address << 1);
	sii9022_stop();

	return ret;
}

static int sii9022_recover_bus(void)
{
	int i;
	int ret;

	/* Release an interrupted I2C slave before interpreting any ACK bit. */
	sii9022_sda(1);
	for (i = 0; i < 9; i++) {
		sii9022_scl(0);
		sii9022_delay();
		ret = sii9022_scl(1);
		if (ret)
			return ret;
		sii9022_delay();
	}
	sii9022_stop();

	return sii9022_gpio_get(SII9022_SDA) ? 0 : -EBUSY;
}

static int sii9022_find_address(void)
{
	u8 address;

	/* The dedicated transmitter bus may strap the address away from 0x39. */
	for (address = 0x30; address <= 0x3f; address++) {
		if (!sii9022_probe(address)) {
			sii9022_i2c_addr = address;
			return 0;
		}
	}

	return -ENXIO;
}

static int sii9022_write_timing(void)
{
	/* SiI9022 TPI video data: 640x480@60, 25.175 MHz, RGB888 1x. */
	static const struct {
		u8 reg;
		u8 value;
	} setup[] = {
		{ SII9022_REG_PIXEL_CLK_LSB, 0xd5 },
		{ SII9022_REG_PIXEL_CLK_MSB, 0x09 },
		{ SII9022_REG_VERT_FREQ_LSB, 60 },
		{ SII9022_REG_VERT_FREQ_MSB, 0x00 },
		{ SII9022_REG_TOTAL_PIXELS_LSB, 0x80 },
		{ SII9022_REG_TOTAL_PIXELS_MSB, 0x02 },
		{ SII9022_REG_TOTAL_LINES_LSB, 0xe0 },
		{ SII9022_REG_TOTAL_LINES_MSB, 0x01 },
		{ SII9022_REG_INPUT_BUS, 0x60 },
		{ SII9022_REG_INPUT_FORMAT, 0x00 },
		{ SII9022_REG_POWER, 0x00 },
		/* Clear SYS_CTRL.PWR_DWN only after the D0 power-state request. */
		{ SII9022_REG_SYS_CTRL, 0x01 },
	};
	int i;
	int ret;

	ret = sii9022_write(SII9022_REG_TPI_ENABLE, 0x00);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(setup); i++) {
		ret = sii9022_write(setup[i].reg, setup[i].value);
		if (ret)
			return ret;
	}

	return 0;
}

static void sii9022_report_video_state(void)
{
	u8 value[11];
	u8 sys_ctrl, power, tpi_enable;
	int i;
	int ret;

	printf("SII9022: RGB input 640x480@60, RGB888, falling-edge, 1x clock\n");
	printf("SII9022: pixel clock 25.175 MHz, htotal 800, vtotal 525\n");

	for (i = 0; i < ARRAY_SIZE(value); i++) {
		ret = sii9022_read(i, &value[i]);
		if (ret) {
			printf("SII9022: TPI read 0x%02x failed (%d)\n", i, ret);
			return;
		}
	}
	ret = sii9022_read(SII9022_REG_SYS_CTRL, &sys_ctrl);
	if (!ret)
		ret = sii9022_read(SII9022_REG_POWER, &power);
	if (!ret)
		ret = sii9022_read(SII9022_REG_TPI_ENABLE, &tpi_enable);
	if (ret) {
		printf("SII9022: control readback failed (%d)\n", ret);
		return;
	}

	printf("SII9022: TPI[00-0a] %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       value[0], value[1], value[2], value[3], value[4], value[5],
	       value[6], value[7], value[8], value[9], value[10]);
	printf("SII9022: TPI C7=%02x 1A=%02x 1E=%02x\n",
	       tpi_enable, sys_ctrl, power);
}

int sii9022_bootloader_setup(void)
{
	u8 id0, id1, id2;
	int ret;

	sunxi_gpio_set_pull(SII9022_SDA, SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SII9022_SCL, SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SII9022_INT, SUNXI_GPIO_PULL_UP);
	sii9022_gpio_input(SII9022_INT);

	sii9022_gpio_output(SII9022_RESET, 0);
	mdelay(10);
	sii9022_gpio_output(SII9022_RESET, 1);
	mdelay(500);

	sii9022_sda(1);
	ret = sii9022_scl(1);
	printf("SII9022: before recovery SDA %d SCL %d HPD %d\n",
	       sii9022_gpio_get(SII9022_SDA), sii9022_gpio_get(SII9022_SCL),
	       sii9022_gpio_get(SII9022_INT));
	if (ret) {
		printf("SII9022: SCL release failed (%d)\n", ret);
		return ret;
	}
	ret = sii9022_recover_bus();
	printf("SII9022: after recovery SDA %d SCL %d\n",
	       sii9022_gpio_get(SII9022_SDA), sii9022_gpio_get(SII9022_SCL));
	if (ret) {
		printf("SII9022: SDA remains held low (%d)\n", ret);
		return ret;
	}

	ret = sii9022_find_address();
	if (ret) {
		printf("SII9022: no device responded at 0x30-0x3f (%d)\n", ret);
		return ret;
	}

	ret = sii9022_write(SII9022_REG_TPI_ENABLE, 0x00);
	if (ret) {
		printf("SII9022: TPI request failed (%d)\n", ret);
		return ret;
	}

	ret = sii9022_read(0x1b, &id0);
	if (!ret)
		ret = sii9022_read(0x1c, &id1);
	if (!ret)
		ret = sii9022_read(0x1d, &id2);
	if (ret) {
		printf("SII9022: register read failed at 0x%02x (%d)\n",
		       sii9022_i2c_addr, ret);
		return ret;
	}

	printf("SII9022: addr 0x%02x id %02x %02x %02x, HPD %d\n",
	       sii9022_i2c_addr, id0, id1, id2,
	       sii9022_gpio_get(SII9022_INT));
	if (id0 != 0xb0) {
		printf("SII9022: invalid TPI device ID %02x (expected b0)\n",
		       id0);
		return -ENODEV;
	}
	ret = sii9022_write_timing();
	if (ret)
		printf("SII9022: configuration failed (%d)\n", ret);
	else {
		printf("SII9022: 640x480@60 HDMI output enabled\n");
		sii9022_report_video_state();
	}

	return ret;
}
