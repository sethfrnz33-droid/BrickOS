# Requires devkitPro's 3ds-dev toolchain.
ifeq ($(strip $(DEVKITARM)),)
$(error Set DEVKITARM to your devkitARM installation)
endif
include $(DEVKITARM)/3ds_rules
TARGET := BrickECU-3DS-Probe
BUILD := build
SOURCES := source
INCLUDES :=
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__
LDFLAGS := -specs=3dsx.specs -g $(ARCH)
LIBS := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export LD := $(CC)
export APP_TITLE := Brick Tuner Suite
export APP_DESCRIPTION := Brick ECU wireless tuning suite
export APP_AUTHOR := Brick ECU
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
# Use an explicit source manifest.  Other projects (e.g. garden or instrument
# demos) can leave .c/.S files in source/ during an in-place upgrade; wildcard
# discovery would silently link them and could duplicate embedded font symbols.
# Add newly approved Brick Tuner modules to this manifest when introduced.
export OFILES := main.o font_assets.o ts_icons.o event_audio_assets.o mascot_assets.o
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh
.PHONY: all clean
all: art/dreamfont.t3x art/tsicons.t3x art/mascot.t3x $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
art/dreamfont.t3x: art/dreamfont.t3s $(wildcard art/type/*.png)
	tex3ds -i art/dreamfont.t3s -o $@
art/tsicons.t3x: art/tsicons.t3s $(wildcard art/tsicons/*.png)
	tex3ds -i art/tsicons.t3s -o $@
art/mascot.t3x: art/mascot.t3s art/mascot/seed.txt tools/generate_mascot.py
	python3 tools/generate_mascot.py
	tex3ds -i art/mascot.t3s -o $@
$(BUILD):
	@mkdir -p $@
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh art/dreamfont.t3x art/tsicons.t3x art/mascot.t3x
else
$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf : $(OFILES)
-include $(DEPSDIR)/*.d
endif
