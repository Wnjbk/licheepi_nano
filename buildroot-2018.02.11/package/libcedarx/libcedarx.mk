################################################################################
#
# LIBCEDARX
#
################################################################################
LIBCEDARX_VERSION = 6215f1c9465d2f23f02930957a0bacdda0878bb1
LIBCEDARX_SITE = https://github.com/rhodesepass/libcedarx.git
LIBCEDARX_SITE_METHOD = git
LIBCEDARX_DEPENDENCIES = openssl libcedarc
LIBCEDARX_INSTALL_STAGING = YES
LIBCEDARX_INSTALL_TARGET = YES
LIBCEDARX_AUTORECONF = YES
LIBCEDARX_ARCHLIB = openwrt-arm926-glibc
LIBCEDARX_CONF_ENV = \
	CFLAGS="$(TARGET_CFLAGS) -D__ENABLE_ZLIB__ -Wno-error=tautological-compare -Wno-error=maybe-uninitialized" \
	CPPFLAGS="$(TARGET_CXXFLAGS) -D__ENABLE_ZLIB__ -Wno-error=tautological-compare -Wno-error=maybe-uninitialized" \
	LDFLAGS="$(TARGET_LDFLAGS) -L$(@D)/external/lib32/$(LIBCEDARX_ARCHLIB) -L$(STAGING_DIR)/usr/lib -lcrypto -lz -lssl" 
LIBCEDARX_CONF_OPTS = \
	--enable-file-stream \
	--enable-mov-parser
LIBCEDARX_MAKE_ENV = 

LIBCEDARX_INSTALL_STAGING_CMDS = $(TARGET_MAKE_ENV) $(MAKE) DESTDIR=$(STAGING_DIR) -C $(@D) install;

LIBCEDARX_INSTALL_STAGING_CMDS += cp $(@D)/libcore/base/include/* '$(STAGING_DIR)/usr/include/';
LIBCEDARX_INSTALL_STAGING_CMDS += cp $(@D)/libcore/parser/include/* '$(STAGING_DIR)/usr/include/';
LIBCEDARX_INSTALL_STAGING_CMDS += cp $(@D)/libcore/stream/include/* '$(STAGING_DIR)/usr/include/';
LIBCEDARX_INSTALL_STAGING_CMDS += cp $(@D)/external/include/adecoder/* '$(STAGING_DIR)/usr/include/';


LIBCEDARX_INSTALL_TARGET_CMDS = $(TARGET_MAKE_ENV) $(MAKE) DESTDIR=$(TARGET_DIR) -C $(@D) install;
# LIBCEDARX_INSTALL_TARGET_CMDS += cp '$(@D)/library/$(LIBCEDARX_ARCHLIB)/libVE.so' '$(TARGET_DIR)/usr/lib/';
# LIBCEDARX_INSTALL_TARGET_CMDS += cp '$(@D)/library/$(LIBCEDARX_ARCHLIB)/libVE.so' '$(HOST_DIR)/arm-buildroot-linux-gnueabi/sysroot/lib/';
# LIBCEDARX_INSTALL_TARGET_CMDS += cp '$(@D)/library/$(LIBCEDARX_ARCHLIB)/libvideoengine.so' '$(TARGET_DIR)/usr/lib/';
# LIBCEDARX_INSTALL_TARGET_CMDS += cp '$(@D)/library/$(LIBCEDARX_ARCHLIB)/libvideoengine.so' '$(HOST_DIR)/arm-buildroot-linux-gnueabi/sysroot/lib/';

$(eval $(autotools-package))
