# Separate lightweight DS-mode .nds for Nintendo DSi, built with devkitPro nds-dev.
# The existing 3DS Makefile at project root is not modified.
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; install devkitPro nds-dev and use devkitPro MSYS2)
endif
include $(DEVKITARM)/ds_rules
TARGET := Brick-Tuner-Suite-DSi
BUILD := build
SOURCES := source
ARCH := -march=armv5te -mtune=arm946e-s -mthumb
CFLAGS := -g -Wall -Wextra -O2 -ffunction-sections -fdata-sections $(ARCH) $(INCLUDE) -DARM9 -DBTS_NATIVE_NDS
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lfat -ldswifi9 -lnds9 -lm
LIBDIRS := $(LIBNDS) $(PORTLIBS)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export LD := $(CC)
export OFILES := main.o surface3d.o tuner.o wire.o storage.o sfx.o dashboard.o
export INCLUDE := $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/source -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export GAME_TITLE := Brick Tuner Suite
export GAME_SUBTITLE1 := DSi / ECU INI
export GAME_SUBTITLE2 := Brick ECU
.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $@
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).map
else
$(OUTPUT).nds : $(OUTPUT).elf
$(OUTPUT).elf : $(OFILES)
-include $(DEPSDIR)/*.d
endif
