# Name of the DTS file (without the extension)
DTS = testNode

# Compiler options
DTC = dtc
DTC_FLAGS = -I dts -O dtb

# Default target
all: $(DTS).dtb

# Rule to compile DTS to DTB
%.dtb: %.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<

# Clean target to remove generated files
clean:
	@rm -f *.dtb

# Phony targets
.PHONY: all clean
