noinst_LTLIBRARIES = libcomposite.la

#AM_CFLAGS = $(DIX_CFLAGS)
AM_CFLAGS = \
	-DHAVE_XORG_CONFIG_H \
	@DIX_CFLAGS@ @XORG_CFLAGS@


if XORG
sdk_HEADERS = compositeext.h
endif

libcomposite_la_SOURCES = 	\
	compalloc.c		\
	compext.c		\
	compint.h		\
	compinit.c		\
	compoverlay.c		\
	compwindow.c
