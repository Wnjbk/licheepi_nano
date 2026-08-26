################################################################################
#
# rtl8723ds
#
################################################################################

RTL8723DS_VERSION = local
RTL8723DS_SITE = $(TOPDIR)/../third_party/rtl8723ds
RTL8723DS_SITE_METHOD = local
RTL8723DS_LICENSE = GPL-2.0

RTL8723DS_MODULE_MAKE_OPTS = \
	KVER=$(LINUX_VERSION_PROBED) \
	KSRC=$(LINUX_DIR) \
	ARCH=arm \
	CROSS_COMPILE=$(TARGET_CROSS)

$(eval $(kernel-module))
$(eval $(generic-package))
