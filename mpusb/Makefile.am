#

bin_PROGRAMS = mpusb
lib_LTLIBRARIES = libmpusb.la

mpusb_SOURCES = main.c main.h
mpusb_LDADD = libmpusb.la

libmpusb_la_SOURCES = mpusb.c mpusb.h debug.c debug.h usb-transport.c usb-transport.h \
	usb-pic-driver.c usb-pic-driver.h usb-avr-driver.c usb-avr-driver.h


library_includedir=$(includedir)/mpusb
library_include_HEADERS = mpusb.h
