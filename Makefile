# Hybrid Makefile for AVR Projects
# Optimized for size and includes diagnostic outputs

MCU      = atmega328p
F_CPU    = 1000000UL
TARGET   = $(notdir $(CURDIR))
FORMAT   = ihex

# Directories
SRC_DIR   = src
BUILD_DIR = build

# Recursive Search for Source Files (Supports subdirectories like src/drivers)
SRCS      = $(shell find $(SRC_DIR) -name "*.c")
OBJS      = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Toolchain
CC       = avr-gcc
OBJCOPY  = avr-objcopy
OBJDUMP  = avr-objdump
SIZE     = avr-size
NM       = avr-nm
AVRDUDE  = avrdude

# Programming Settings (USBasp)
BITCLOCK = 5.33
PROGRAMMER = usbasp
PORT = usb

# Fuses (Internal 8MHz RC + CKDIV8 = 1MHz)
LFUSE    = 0x62
HFUSE    = 0xD9
EFUSE    = 0xFF

# Compiler Flags
# -Os: Optimize for size
# -ffunction-sections -fdata-sections: Prepare for dead-code elimination
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c11 \
           -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
           -ffunction-sections -fdata-sections -I$(SRC_DIR)

# Linker Flags
# --gc-sections: Remove unused code (requires CFLAGS above)
LDFLAGS  = -mmcu=$(MCU) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
           -Wl,--gc-sections -lm

all: build size
	@echo "------------------------------------------------"
	@echo "Build successful! Run 'make program' to flash."
	@echo "------------------------------------------------"

build: $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).lss $(BUILD_DIR)/$(TARGET).sym
	@echo "Done building diagnostic files (.lss, .sym)."

# Create Hex file for flashing
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@echo "Generating Hex: $@"
	@$(OBJCOPY) -O $(FORMAT) -R .eeprom -R .fuse -R .lock $< $@

# Link objects into ELF
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(dir $@)
	@echo "Linking: $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Compile C files to objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Generate Assembly listing
$(BUILD_DIR)/$(TARGET).lss: $(BUILD_DIR)/$(TARGET).elf
	@$(OBJDUMP) -h -S -z $< > $@

# Generate Symbol table
$(BUILD_DIR)/$(TARGET).sym: $(BUILD_DIR)/$(TARGET).elf
	@$(NM) -n $< > $@

# Display memory usage
size: $(BUILD_DIR)/$(TARGET).elf
	@echo ""
	@echo "Memory Usage Summary:"
	@$(SIZE) --mcu=$(MCU) --format=avr $<
	@echo ""

# Flash the hex file using USBasp
program: $(BUILD_DIR)/$(TARGET).hex
	@echo "Starting flash process with USBasp..."
	$(AVRDUDE) -v -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -B $(BITCLOCK) -U flash:w:$<
	@echo "------------------------------------------------"
	@echo "Flash complete and verified!"
	@echo "------------------------------------------------"

# Set the fuse bytes
fuse:
	@echo "Setting fuses (1MHz internal oscillator)..."
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -B $(BITCLOCK) \
		-U lfuse:w:$(LFUSE):m \
		-U hfuse:w:$(HFUSE):m \
		-U efuse:w:$(EFUSE):m
	@echo "Fuses set successfully."

# Initialize a new project structure in the current directory
init:
	@echo "Initializing new AVR project..."
	@mkdir -p src/drivers
	@mkdir -p build
	@if [ ! -f src/main.c ]; then \
		echo '#include <avr/io.h>\n#include <util/delay.h>\n\nint main(void) {\n    DDRB |= (1 << PB5);\n    while(1) {\n        PORTB ^= (1 << PB5);\n        _delay_ms(500);\n    }\n    return 0;\n}' > src/main.c; \
		echo "Created src/main.c (Blink example)"; \
	fi
	@if [ ! -f .gitignore ]; then \
		echo "build/\n*.elf\n*.hex\n*.obj\n*.o\n*.d\n*.eep\n*.lst\n*.lss\n*.sym\n*.map" > .gitignore; \
		echo "Created .gitignore"; \
	fi
	@cp -n src/drivers/millis.c src/drivers/millis.c 2>/dev/null || true
	@cp -n src/drivers/millis.h src/drivers/millis.h 2>/dev/null || true
	@echo "Project structure ready."
	@if [ ! -d .git ]; then \
		git init && git add . && git commit -m "Initial commit (AVR Project Template)"; \
		echo "Initialized Git repository."; \
	fi

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all build size clean program fuse
