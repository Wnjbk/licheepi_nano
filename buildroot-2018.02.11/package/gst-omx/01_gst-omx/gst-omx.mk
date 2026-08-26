################################################################################
#
# gst-omx
#
################################################################################

GST_OMX_VERSION = 1.12.4
GST_OMX_SOURCE = gst-omx-$(GST_OMX_VERSION).tar.xz
GST_OMX_SITE = https://gstreamer.freedesktop.org/src/gst-omx

GST_OMX_LICENSE = LGPL-2.1+
GST_OMX_LICENSE_FILES = COPYING
GST_OMX_DEPENDENCIES = gstreamer1 gst1-plugins-base gst1-plugins-bad

GST_OMX_CONF_OPTS = \
	--with-omx-target=generic \
	--disable-valgrind \
	--disable-examples

GST_OMX_CONF_ENV = \
	CFLAGS="$(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include"

$(eval $(autotools-package))
