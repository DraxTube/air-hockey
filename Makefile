ifeq ($(strip $(DEVKITARM)),)
$(warning "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
DEVKITARM := /opt/devkitpro/devkitARM
endif

include $(DEVKITARM)/ds_rules

TARGET		:= airhockey
BUILD		:= build
SOURCES		:= source

CFLAGS		:= -g -Wall -O2 \
			   -march=armv5te -mtune=arm946e-s -mthumb -mthumb-interwork \
			   -I$(DEVKITPRO)/libnds/include -DARM9

LDFLAGS		:= -specs=ds_arm9.specs -g \
			   -mthumb -mthumb-interwork

LIBS		:= -lnds9 -lm
LIBDIRS		:= $(DEVKITPRO)/libnds

vpath %.c $(SOURCES)
vpath %.cpp $(SOURCES)

CFILES		:= $(wildcard $(SOURCES)/*.c)
OFILES		:= $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES))

all: $(TARGET).nds

$(TARGET).nds: $(TARGET).elf
	ndstool -c $@ -9 $<

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) -L$(LIBDIRS)/lib $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf

