// SPDX-License-Identifier: GPL-2.0+
/*
 * ST7701S 3-wire 9-bit SPI init for the current 384x640 RGB565 no-DE/HV-sync panel.
 *
 * This helper only wakes and configures the panel. The actual U-Boot
 * framebuffer console still comes from the legacy sunxi LCD path.
 *
 * Pin mapping for the repaired board test:
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

static void st7701_shift_9bit(int is_data, u8 value)
{
	pa_set_bit(BIT_MOSI, is_data);
	pa_set_bit(BIT_SCLK, 0);
	spi_delay();
	pa_set_bit(BIT_SCLK, 1);
	spi_delay();
	spi_send_byte(value);
}

static void st7701_write(u8 cmd, const u8 *data, int len)
{
	int i;

	pa_set_bit(BIT_CS, 0);
	st7701_shift_9bit(0, cmd);
	for (i = 0; i < len; i++)
		st7701_shift_9bit(1, data[i]);
	pa_set_bit(BIT_CS, 1);
}

static void st7701_panel_reset(void)
{
	pa_set_bit(BIT_CS, 1);
	pa_set_bit(BIT_SCLK, 0);
	pa_set_bit(BIT_MOSI, 0);
	pa_set_bit(BIT_RST, 0);
	spi_mdelay(120);
	pa_set_bit(BIT_RST, 1);
	spi_mdelay(120);
}

static void st7701_init_seq(void)
{
	static const u8 ff_13[] = { 0x77, 0x01, 0x00, 0x00, 0x13 };
	static const u8 ef_08[] = { 0x08 };
	static const u8 ff_10[] = { 0x77, 0x01, 0x00, 0x00, 0x10 };
	static const u8 c0[] = { 0x4F, 0x00 };
	static const u8 c1[] = { 0x07, 0x02 };
	static const u8 c2[] = { 0x31, 0x05 };
	static const u8 c7[] = { 0x04 };
	static const u8 c3[] = { 0x82, 0x18, 0x0E };
	static const u8 cc[] = { 0x10 };
	static const u8 b0[] = {
		0x0A, 0x14, 0x1B, 0x0D,
		0x10, 0x05, 0x07, 0x08,
		0x06, 0x22, 0x03, 0x11,
		0x10, 0xAD, 0x31, 0x1B
	};
	static const u8 b1[] = {
		0x0A, 0x14, 0x1B, 0x0D,
		0x10, 0x05, 0x07, 0x08,
		0x06, 0x22, 0x03, 0x11,
		0x10, 0xAD, 0x31, 0x1B
	};
	static const u8 ff_11[] = { 0x77, 0x01, 0x00, 0x00, 0x11 };
	static const u8 e0[] = { 0x00, 0x00, 0x02 };
	static const u8 e1[] = {
		0x03, 0xA0, 0x00, 0x00,
		0x02, 0xA0, 0x00, 0x00,
		0x00, 0x33, 0x33
	};
	static const u8 e2[] = {
		0x22, 0x22, 0x33, 0x33,
		0x88, 0xA0, 0x00, 0x00,
		0x87, 0xA0, 0x00, 0x00
	};
	static const u8 e3[] = { 0x00, 0x00, 0x22, 0x22 };
	static const u8 e4[] = { 0x44, 0x44 };
	static const u8 e5[] = {
		0x04, 0x84, 0xA0, 0xA0,
		0x06, 0x86, 0xA0, 0xA0,
		0x08, 0x88, 0xA0, 0xA0,
		0x0A, 0x8A, 0xA0, 0xA0
	};
	static const u8 e6[] = { 0x00, 0x00, 0x22, 0x22 };
	static const u8 e7[] = { 0x44, 0x44 };
	static const u8 e8[] = {
		0x03, 0x83, 0xA0, 0xA0,
		0x05, 0x85, 0xA0, 0xA0,
		0x07, 0x87, 0xA0, 0xA0,
		0x09, 0x89, 0xA0, 0xA0
	};
	static const u8 eb[] = { 0x00, 0x01, 0xE4, 0xE4, 0x88, 0x00, 0x40 };
	static const u8 ec[] = { 0x3C, 0x01 };
	static const u8 ed[] = {
		0xAB, 0x89, 0x76, 0x54,
		0x02, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0x20,
		0x45, 0x67, 0x98, 0xBA
	};
	static const u8 b0_53[] = { 0x53 };
	static const u8 b1_60[] = { 0x60 };
	static const u8 b2_07[] = { 0x07 };
	static const u8 b3_80[] = { 0x80 };
	static const u8 b5_49[] = { 0x49 };
	static const u8 b7_85[] = { 0x85 };
	static const u8 b8_21[] = { 0x21 };
	static const u8 c1_78[] = { 0x78 };
	static const u8 c2_78[] = { 0x78 };
	static const u8 madctl[] = { 0x10 };
	static const u8 e8_0e[] = { 0x00, 0x0E };
	static const u8 pixfmt[] = { 0x50 };
	static const u8 e8_0c[] = { 0x00, 0x0C };
	static const u8 e8_00[] = { 0x00, 0x00 };
	static const u8 ff_00[] = { 0x77, 0x01, 0x00, 0x00, 0x00 };

	st7701_write(0xFF, ff_13, ARRAY_SIZE(ff_13));
	st7701_write(0xEF, ef_08, ARRAY_SIZE(ef_08));

	st7701_write(0xFF, ff_10, ARRAY_SIZE(ff_10));
	st7701_write(0xC0, c0, ARRAY_SIZE(c0));
	st7701_write(0xC1, c1, ARRAY_SIZE(c1));
	st7701_write(0xC2, c2, ARRAY_SIZE(c2));
	st7701_write(0xC7, c7, ARRAY_SIZE(c7));
	st7701_write(0xC3, c3, ARRAY_SIZE(c3));
	st7701_write(0xCC, cc, ARRAY_SIZE(cc));
	st7701_write(0xB0, b0, ARRAY_SIZE(b0));
	st7701_write(0xB1, b1, ARRAY_SIZE(b1));

	st7701_write(0xFF, ff_11, ARRAY_SIZE(ff_11));
	st7701_write(0xB0, b0_53, ARRAY_SIZE(b0_53));
	st7701_write(0xB1, b1_60, ARRAY_SIZE(b1_60));
	st7701_write(0xB2, b2_07, ARRAY_SIZE(b2_07));
	st7701_write(0xB3, b3_80, ARRAY_SIZE(b3_80));
	st7701_write(0xB5, b5_49, ARRAY_SIZE(b5_49));
	st7701_write(0xB7, b7_85, ARRAY_SIZE(b7_85));
	st7701_write(0xB8, b8_21, ARRAY_SIZE(b8_21));
	st7701_write(0xC1, c1_78, ARRAY_SIZE(c1_78));
	st7701_write(0xC2, c2_78, ARRAY_SIZE(c2_78));
	st7701_write(0xE0, e0, ARRAY_SIZE(e0));
	st7701_write(0xE1, e1, ARRAY_SIZE(e1));
	st7701_write(0xE2, e2, ARRAY_SIZE(e2));
	st7701_write(0xE3, e3, ARRAY_SIZE(e3));
	st7701_write(0xE4, e4, ARRAY_SIZE(e4));
	st7701_write(0xE5, e5, ARRAY_SIZE(e5));
	st7701_write(0xE6, e6, ARRAY_SIZE(e6));
	st7701_write(0xE7, e7, ARRAY_SIZE(e7));
	st7701_write(0xE8, e8, ARRAY_SIZE(e8));
	st7701_write(0xEB, eb, ARRAY_SIZE(eb));
	st7701_write(0xEC, ec, ARRAY_SIZE(ec));
	st7701_write(0xED, ed, ARRAY_SIZE(ed));

	st7701_write(0x36, madctl, ARRAY_SIZE(madctl));
	st7701_write(0xFF, ff_13, ARRAY_SIZE(ff_13));
	st7701_write(0xE8, e8_0e, ARRAY_SIZE(e8_0e));
	st7701_write(0x20, NULL, 0);
	st7701_write(0x3A, pixfmt, ARRAY_SIZE(pixfmt));
	st7701_write(0x11, NULL, 0);
	spi_mdelay(150);

	st7701_write(0xE8, e8_0c, ARRAY_SIZE(e8_0c));
	spi_mdelay(100);

	st7701_write(0xE8, e8_00, ARRAY_SIZE(e8_00));
	st7701_write(0xFF, ff_00, ARRAY_SIZE(ff_00));
	st7701_write(0x29, NULL, 0);
	spi_mdelay(20);
}

int st7701_spi_bootloader_setup(void)
{
	printf("ST7701: init 384x640 RGB565 no-DE/HV-sync panel\n");

	writel((1 << 0) | (1 << 4) | (1 << 8) | (1 << 12), (void *)PIO_CFG0);
	pa_shadow = BIT(BIT_CS);
	writel(pa_shadow, (void *)PIO_DAT);

	st7701_panel_reset();
	st7701_init_seq();
	printf("ST7701: init done\n");

	return 0;
}
