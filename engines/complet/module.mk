MODULE := engines/complet

MODULE_OBJS := \
	complet.o \
	gd.o \
	metaengine.o \
	scenario.o

# This module can be built as a plugin
ifeq ($(ENABLE_COMPLET), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
