MODULE := engines/complet

MODULE_OBJS := \
	audio.o \
	complet.o \
	console.o \
	gallery.o \
	gd.o \
	metaengine.o \
	saveload.o \
	scenario.o \
	script.o

# This module can be built as a plugin
ifeq ($(ENABLE_COMPLET), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
