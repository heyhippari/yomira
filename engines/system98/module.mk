MODULE := engines/system98

MODULE_OBJS := \
	audio.o \
	dialogs.o \
	graphics.o \
	image.o \
	metaengine.o \
	mmd.o \
	pmd.o \
	resource.o \
	script.o \
	system98.o

ifeq ($(ENABLE_SYSTEM98), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

include $(srcdir)/rules.mk

DETECT_OBJS += $(MODULE)/detection.o
