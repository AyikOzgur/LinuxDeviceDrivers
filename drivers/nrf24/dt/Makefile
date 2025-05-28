# Name of the DTS file (without the extension)
DTS = nrf24

# Compiler & flags: note the “-@” to emit __symbols__ and __overrides__
DTC       := dtc
DTC_FLAGS := -@ -I dts -O dtb

# Default target
all: $(DTS).dtbo

# Rule to compile .dts → .dtbo
%.dtbo: %.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<

# Clean up generated overlays
clean:
	@rm -f *.dtbo

.PHONY: all clean