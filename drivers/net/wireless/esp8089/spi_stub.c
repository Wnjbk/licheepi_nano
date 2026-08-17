#define MHz (1000000U)

/* ESP8089 @ 30MHz */
#define SPI_FREQ (30 * MHz)

/* Timing for 30MHz */
#define CMD_RESP_SIZE 10
#define DATA_RESP_SIZE_W (142 + 45)
#define DATA_RESP_SIZE_R (231 + 75)
#define BLOCK_W_DATA_RESP_SIZE_EACH 10
#define BLOCK_W_DATA_RESP_SIZE_FINAL 152
#define BLOCK_R_DATA_RESP_SIZE_1ST 265
#define BLOCK_R_DATA_RESP_SIZE_EACH 10

#include "esp_sif.h"
#include "linux/interrupt.h"
#include "linux/spi/spi.h"
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int esp_spi_bus = 0;       /* physical SPI1 registers as Linux master spi0 */
module_param(esp_spi_bus, int, 0);
static int esp_interrupt = 140;   /* PE12 = 4*32+12 */
module_param(esp_interrupt, int, 0);
static int esp_irq_active_low = 1;
module_param(esp_irq_active_low, int, 0);

#define MAX_SPEED_HZ SPI_FREQ

struct spi_device_id esp_spi_id[] = {
  {"ESP8089_0", 0}, {"ESP8089_1", 1}, {"ESP8089_2", 2}, {},
};
MODULE_DEVICE_TABLE(spi, esp_spi_id);

static struct spi_master *master;
static struct spi_device *spi;
static int esp_irq_no = -1;
static int esp_irq_masked;

static struct spi_board_info esp_board_spi_devices[] = {
  { .modalias = "ESP8089_0", .max_speed_hz = MAX_SPEED_HZ,
    .bus_num = 0, .chip_select = 0, .mode = SPI_MODE_3 },
};

void sif_platform_register_board_info(void) {}

struct spi_device* sif_platform_new_device(void) {
  esp_board_spi_devices[0].bus_num = esp_spi_bus;
  master = spi_busnum_to_master(esp_board_spi_devices[0].bus_num);
  if (!master) { printk("esp8089_spi: SPI%d not found\n", esp_spi_bus); return NULL; }
  spi = spi_new_device(master, esp_board_spi_devices);
  if (!spi || spi_setup(spi)) { printk("esp8089_spi: setup fail\n"); return NULL; }
  printk("esp8089_spi: ready on SPI%d, %dHz\n", esp_spi_bus, MAX_SPEED_HZ);
  return spi;
}

int sif_platform_irq_init(void) {
  int ret;
  if ((ret = gpio_request(esp_interrupt, "esp_int")) != 0)
    { printk("esp8089_spi: gpio %d request fail\n", esp_interrupt); return ret; }
  gpio_direction_input(esp_interrupt);
  esp_irq_no = gpio_to_irq(esp_interrupt);
  if (esp_irq_no < 0) {
    printk("esp8089_spi: gpio_to_irq(%d) failed: %d\n", esp_interrupt, esp_irq_no);
    gpio_free(esp_interrupt);
    return esp_irq_no;
  }
  esp_irq_masked = 0;
  udelay(2);
  return 0;
}
void sif_platform_irq_deinit(void) { esp_irq_no = -1; esp_irq_masked = 0; gpio_free(esp_interrupt); }
int  sif_platform_get_irq_no(void)   { return esp_irq_no; }
int  sif_platform_is_irq_occur(void) {
  int level = gpio_get_value(esp_interrupt);
  return esp_irq_active_low ? !level : !!level;
}
void sif_platform_irq_clear(void) {}
void sif_platform_irq_mask(int mask) {
  if (esp_irq_no < 0)
    return;
  if (mask) {
    if (!esp_irq_masked) {
      disable_irq_nosync(esp_irq_no);
      esp_irq_masked = 1;
    }
  } else {
    if (esp_irq_masked) {
      enable_irq(esp_irq_no);
      esp_irq_masked = 0;
    }
  }
}
void sif_platform_target_speed(int high_speed) {}
#ifdef ESP_ACK_INTERRUPT
void sif_platform_ack_interrupt(struct esp_pub *epub) { sif_platform_irq_clear(); }
#endif

/* No hardware reset on this board */
void sif_platform_reset_target(void) {}
void sif_platform_target_poweroff(void) {}
void sif_platform_target_poweron(void) {}

late_initcall(esp_spi_init);
module_exit(esp_spi_exit);
