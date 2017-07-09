## sub directory

h_sources = \
	usbinterface.h \
	usbsocket.h \
	usbconnector.h

cc_sources = \
	usbinterface.cpp \
	usbsocket.cpp \
	usbconnector.cpp

#Install the headers in a versioned directory
library_includedir = $(includedir)/$(GENERIC_LIBRARY_NAME)-$(GENERIC_API_VERSION)/$(GENERIC_LIBRARY_NAME)/usb
library_include_HEADERS = $(h_sources)

AUTOMAKE_OPTIONS = nostdinc
AM_CPPFLAGS = -I$(top_srcdir) $(LIBUSB_CFLAGS) $(GCOV_CFLAGS)
AM_LDFLAGS = $(LIBUSB_LIBS) $(GCOV_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libibrcommon_usb.la
libibrcommon_usb_la_SOURCES = $(h_sources) $(cc_sources)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT $(GENERIC_LIBRARY_NAME) \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libibrcommon_usb \
		-:SOURCES $(h_sources) $(cc_sources) \
		-:CPPFLAGS $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		-:SUBDIR $(patsubst %,ibrcommon/usb/%, $(SUBDIRS)) \
		> $@
