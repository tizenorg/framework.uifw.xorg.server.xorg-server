noinst_LTLIBRARIES = libhwc.la
AM_CFLAGS = \
	-DHAVE_XORG_CONFIG_H \
	@DIX_CFLAGS@ @XORG_CFLAGS@

libhwc_la_SOURCES = \
	hwc.h \
	hwc_priv.h \
	hwc.c \
	hwc_request.c \
	hwc_event.c \
	hwc_screen.c

sdk_HEADERS = hwc.h
