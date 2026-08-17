ccflags-y += -DFAST_TX_STATUS -DRX_SENDUP_SYNC
ccflags-y += -DKERNEL_IV_WAR -DSIF_DSR_WAR
ccflags-y += -DHAS_INIT_DATA
ccflags-y += -DP2P_CONCURRENT
ccflags-y += -DHAS_FW
ccflags-y += -DESP_ACK_INTERRUPT
ccflags-y += -DESP_USE_SPI
ccflags-y += -DREGISTER_SPI_BOARD_INFO

obj-$(CONFIG_ESP8089_SPI) += esp8089-spi.o

esp8089-spi-y := esp_debug.o sdio_sif_esp.o spi_sif_esp.o esp_io.o \
	esp_file.o esp_main.o esp_sip.o esp_ext.o esp_ctrl.o \
	esp_mac80211.o esp_utils.o esp_pm.o testmode.o
