#
# Makefile.am -- oacapture Makefile template
#
# Copyright 2013,2014,2015,2016,2017,2018,2019,2020,2021,2023
#   James Fidell (james@openastroproject.org)
#
# License:
#
# This file is part of the Open Astro Project.
#
# The Open Astro Project is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# The Open Astro Project is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Open Astro Project.  If not, see
# <http://www.gnu.org/licenses/>.
#

#AM_CPPFLAGS = -I$(top_srcdir)/include $(FFMPEG_CFLAGS) $(APP_PATH_CFLAGS) \
#  $(DEFAULT_PATH_CFLAGS) $(LIBWINDIB_CFLAGS) -D__STDC_CONSTANT_MACROS \
#  $(LIBUVC_CFLAGS) $(LIBHIDAPI_CFLAGS) $(LIBUSB_CFLAGS) $(LIBDC1394_CFLAGS) \
#  $(LIBFTDI_CFLAGS)

SUBDIRS = icons desktop translations

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/common $(FFMPEG_CFLAGS) \
	$(APP_PATH_CFLAGS) $(DEFAULT_PATH_CFLAGS) $(LIBWINDIB_CFLAGS) \
	-D__STDC_CONSTANT_MACROS -DTRANSLATE_DIR=\"${datadir}/$(bin_PROGRAMS)/\" \
	-DOACAPTURE

MOC_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/common $(MOC_CFLAGS) \
	$(APP_PATH_CFLAGS) $(DEFAULT_PATH_CFLAGS) -D__STDC_CONSTANT_MACROS \
  -DTRANSLATE_DIR=\"${datadir}/$(bin_PROGRAMS)/\" -DOACAPTURE

bin_PROGRAMS = oacapture
BUILT_SOURCES = version.h
oacapture_SOURCES = cameraWidget.cc captureWidget.cc controlWidget.cc \
  imageWidget.cc mainWindow.cc oacapture.cc previewWidget.cc zoomWidget.cc \
	trampoline.cc displayWindow.cc \
	moc_cameraWidget.cc moc_captureWidget.cc moc_controlWidget.cc \
	moc_displayWindow.cc moc_imageWidget.cc moc_mainWindow.cc \
	moc_previewWidget.cc moc_zoomWidget.cc \
  qrc_oacapture.cc occultationWidget.cc moc_occultationWidget.cc

oacapture_LDADD = \
  ../common/liboacommon.la \
  ../liboacam/liboacam.la \
  ../liboaimgproc/liboaimgproc.la \
  ../liboademosaic/liboademosaic.la \
  ../liboaSER/liboaSER.la \
  ../liboavideo/liboavideo.la \
  ../liboafilterwheel/liboafilterwheel.la \
  ../liboaPTR/liboaPTR.la \
  ../liboautil/liboautil.la \
  $(FFMPEG_LIBS) \
  $(LIBWINDIB_LIBS) \
  $(LIBUVC_LIBS) \
  $(LIBHIDAPI_LIBS) \
  $(LIBDC1394_LIBS) \
  $(LIBFTDI_LIBS) \
  $(LIBUSB_LIBS) \
  $(LIBASI2_LIBS) \
  $(LIBZWOFW_LIBS) \
  $(LIBGPHOTO2_LIBS) \
  $(LIBSVBCAMERASDK_LIBS) \
  $(PYLON_LDFLAGS) $(PYLON_LIBS) \
  $(OSX_FRAMEWORKS) \
  -lpthread

WARNINGS = -g -O -Wall -Werror -Wpointer-arith -Wuninitialized -Wsign-compare -Wformat-security -Wno-pointer-sign $(OSX_WARNINGS)

warnings:
	$(MAKE) V=0 CFLAGS='$(WARNINGS)' CXXFLAGS='$(WARNINGS)'
	$(MAKE) V=0 CFLAGS='$(WARNINGS)' CXXFLAGS='$(WARNINGS)' $(check_PROGRAMS)

verbose-warnings:
	$(MAKE) V=1 CFLAGS='$(WARNINGS)' CXXFLAGS='$(WARNINGS)'
	$(MAKE) V=1 CFLAGS='$(WARNINGS)' CXXFLAGS='$(WARNINGS)' $(check_PROGRAMS)

moc_%.cc: %.h
	@MOC@ -o$@ $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(MOC_CPPFLAGS) $<

ui-%.h: %.ui
	@UIC@ -o $@ $<

qrc_%.cc: %.qrc
	$(LN_S) -f $(top_srcdir)/qt-icons .
	@RCC@ -o $@ $<

mostlyclean-local:
	-rm -rf moc_*.cc qrc_*.cc version.h

version.h: version.h.in $(top_srcdir)/version
	-rm -f $@.tmp
	maj=`cat $(top_srcdir)/version | cut -d. -f1`; \
	min=`cat $(top_srcdir)/version | cut -d. -f2`; \
	rev=`cat $(top_srcdir)/version | cut -d. -f3`; \
	sed -e "s/@MAJOR_VERSION@/$$maj/g" -e "s/@MINOR_VERSION@/$$min/g" \
		-e "s/@REVISION@/$$rev/g" >$@.tmp $<
	cmp -s $@.tmp $@ || ( rm -f $@ && mv $@.tmp $@ )
