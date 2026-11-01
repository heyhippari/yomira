MODULE := engines/illusion

MODULE_OBJS = \
	illusion.o \
	newgame.o \
	archive.o \
	renderer.o \
	renderer_tinygl.o \
	model.o \
	console.o \
	metaengine.o

# This module can be built as a plugin
ifeq ($(ENABLE_ILLUSION), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
