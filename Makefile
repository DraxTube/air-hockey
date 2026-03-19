ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

include $(DEVKITPRO)/devkitARM/ds_rules

TARGET		:= airhockey
BUILD		:= build
SOURCES		:= source

ARCH	:= -mthumb -mthumb-interwork -march=armv5te -mtune=arm946e-s
CFLAGS	:= $(ARCH) -O2 -g -Wall \
		   -I$(DEVKITPRO)/libnds/include \
		   -I$(DEVKITPRO)/calico/include \
		   -DARM9 -D__NDS__

LDFLAGS	:= $(ARCH) -g -specs=$(DEVKITPRO)/calico/share/ds9.specs

# Il nome corretto è libcalico_ds9.a, quindi -lcalico_ds9
LIBS	:= -lnds9 -lcalico_ds9 -lm
LIBDIRS	:= $(DEVKITPRO)/libnds/lib $(DEVKITPRO)/calico/lib

vpath %.c $(SOURCES)

CFILES		:= $(wildcard $(SOURCES)/*.c)
OFILES		:= $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES))

all: $(TARGET).nds

%.arm9: %.elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).nds: $(TARGET).arm9
	ndstool -c $@ -9 $<

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) $(addprefix -L,$(LIBDIRS)) $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf $(TARGET).arm9
