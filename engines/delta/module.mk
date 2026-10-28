MODULE := engines/delta

MODULE_OBJS = \
	delta.o \
	data.o \
	script.o \
	transition.o \
	options.o \
	optionsmenu.o \
	saveload.o \
	console.o \
	metaengine.o

# This module can be built as a plugin
ifeq ($(ENABLE_DELTA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
