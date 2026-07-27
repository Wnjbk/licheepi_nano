// SPDX-License-Identifier: GPL-2.0+
/*
 * ILI9488 3-wire 9-bit SPI init for a 3.5 inch 320x480 RGB panel.
 *
 * This helper only wakes and configures the panel. The actual U-Boot
 * framebuffer console still comes from the legacy sunxi LCD path.
 *
 * Pin mapping kept from the existing RGB panel wiring:
 *   PA0 = CS
 *   PA1 = SDA / MOSI
 *   PA2 = SCL
 *   PA3 = RST
 */

#include <common.h>
#include <asm/io.h>

#define PIO_BASE  0x01C20800
#define PIO_CFG0  (PIO_BASE + 0x00)
#define PIO_DAT   (PIO_BASE + 0x10)

#define BIT_CS    0
#define BIT_SCLK  2
#define BIT_MOSI  1
#define BIT_RST   3

static u32 pa_shadow;

static void spi_delay(void)
{
	volatile int i;

	for (i = 0; i < 100; i++)
		asm volatile("nop");
}

static void spi_mdelay(int ms)
{
	int i, j;

	for (i = 0; i < ms; i++)
		for (j = 0; j < 100000; j++)
			asm volatile("nop");
}

static inline void pa_set_bit(int bit, int val)
{
	if (val)
		pa_shadow |= BIT(bit);
	else
		pa_shadow &= ~BIT(bit);

	writel(pa_shadow, (void *)PIO_DAT);
}

static void spi_send_byte(u8 val)
{
	int n;

	for (n = 0; n < 8; n++) {
		pa_set_bit(BIT_MOSI, !!(val & 0x80));
		val <<= 1;
		pa_set_bit(BIT_SCLK, 0);
		spi_delay();
		pa_set_bit(BIT_SCLK, 1);
		spi_delay();
	}
}

static void ili9488_shift_9bit(int is_data, u8 value)
{
	pa_set_bit(BIT_MOSI, is_data);
	pa_set_bit(BIT_SCLK, 0);
	spi_delay();
	pa_set_bit(BIT_SCLK, 1);
	spi_delay();
	spi_send_byte(value);
}

static void ili9488_write(u8 cmd, const u8 *data, int len)
{
	int i;

	pa_set_bit(BIT_CS, 0);
	ili9488_shift_9bit(0, cmd);
	for (i = 0; i < len; i++)
		ili9488_shift_9bit(1, data[i]);
	pa_set_bit(BIT_CS, 1);
}

static void ili9488_panel_reset(void)
{
	pa_set_bit(BIT_CS, 1);
	pa_set_bit(BIT_SCLK, 0);
	pa_set_bit(BIT_MOSI, 0);
	pa_set_bit(BIT_RST, 0);
	spi_mdelay(120);
	pa_set_bit(BIT_RST, 1);
	spi_mdelay(120);
}

static void ili9488_init_seq(void)
{
	static const u8 e0[] = {
		0x00, 0x1B, 0x1F, 0x0A, 0x0F,
		0x07, 0x3C, 0x38, 0x49, 0x00,
		0x0C, 0x0B, 0x15, 0x17, 0x0F
	};
	static const u8 e1[] = {
		0x00, 0x19, 0x1B, 0x04, 0x0F,
		0x04, 0x30, 0x32, 0x40, 0x01,
		0x07, 0x0E, 0x2B, 0x32, 0x0F
	};
	static const u8 c0[] = { 0x14, 0x12 };
	static const u8 c1[] = { 0x41 };
	static const u8 c5[] = { 0x00, 0x31, 0x80 };
	static const u8 madctl[] = { 0x48 };
	static const u8 pixfmt[] = { 0x66 };
	static const u8 b0[] = { 0x00 };
	static const u8 b1[] = { 0xA0 };
	static const u8 b4[] = { 0x02 };
	static const u8 b6[] = { 0x29, 0x02 };
	static const u8 e9[] = { 0x00 };
	static const u8 f7[] = { 0xA9, 0x51, 0x2C, 0x82 };
	static const u8 zero[] = { 0x00 };

	ili9488_write(0xE0, e0, ARRAY_SIZE(e0));
	ili9488_write(0xE1, e1, ARRAY_SIZE(e1));
	ili9488_write(0xC0, c0, ARRAY_SIZE(c0));
	ili9488_write(0xC1, c1, ARRAY_SIZE(c1));
	ili9488_write(0xC5, c5, ARRAY_SIZE(c5));
	ili9488_write(0x36, madctl, ARRAY_SIZE(madctl));
	ili9488_write(0x3A, pixfmt, ARRAY_SIZE(pixfmt));
	ili9488_write(0xB0, b0, ARRAY_SIZE(b0));
	ili9488_write(0xB1, b1, ARRAY_SIZE(b1));
	ili9488_write(0xB4, b4, ARRAY_SIZE(b4));
	ili9488_write(0xB6, b6, ARRAY_SIZE(b6));
	ili9488_write(0xE9, e9, ARRAY_SIZE(e9));
	ili9488_write(0xF7, f7, ARRAY_SIZE(f7));
	ili9488_write(0x21, NULL, 0);
	spi_mdelay(10);

	ili9488_write(0x11, zero, ARRAY_SIZE(zero));
	spi_mdelay(10);
	ili9488_write(0x29, zero, ARRAY_SIZE(zero));
	spi_mdelay(120);
}

int ili9488_spi_bootloader_setup(void)
{
	printf("ILI9488: init 320x480 RGB panel\n");

	writel((1 << 0) | (1 << 4) | (1 << 8) | (1 << 12), (void *)PIO_CFG0);
	pa_shadow = BIT(BIT_CS);
	writel(pa_shadow, (void *)PIO_DAT);

	ili9488_panel_reset();
	ili9488_init_seq();
	printf("ILI9488: init done\n");

	return 0;
}
