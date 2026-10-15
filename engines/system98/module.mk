MODULE := engines/system98

MODULE_OBJS := \
	metaengine.o \
	system98.o

ifeq ($(ENABLE_SYSTEM98), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

include $(srcdir)/rules.mk

DETECT_OBJS += $(MODULE)/detection.o
