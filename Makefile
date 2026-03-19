ifeq ($(strip $(DEVKITARM)),)
$(warning "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
DEVKITARM := /opt/devkitpro/devkitARM
endif

include $(DEVKITARM)/ds_rules

TARGET		:= airhockey
BUILD		:= build
SOURCES		:= source

# Added calico include paths and specs to resolve modern libnds errors
CFLAGS		:= -g -Wall -O2 \
			   -specs=ds_arm9.specs \
			   -march=armv5te -mtune=arm946e-s -mthumb -mthumb-interwork \
			   -I$(DEVKITPRO)/calico/include \
			   -I$(DEVKITPRO)/libnds/include \
			   -DARM9 -D__NDS__

LDFLAGS		:= -specs=ds_arm9.specs -g \
			   -mthumb -mthumb-interwork

LIBS		:= -lnds9 -lm
LIBDIRS		:= $(DEVKITPRO)/libnds $(DEVKITPRO)/calico

vpath %.c $(SOURCES)
vpath %.cpp $(SOURCES)

CFILES		:= $(wildcard $(SOURCES)/*.c)
OFILES		:= $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES))

all: $(TARGET).nds

%.arm9: %.elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).nds: $(TARGET).arm9
	ndstool -c $@ -9 $<

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) -L$(DEVKITPRO)/libnds/lib -L$(DEVKITPRO)/calico/lib $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf $(TARGET).arm9
