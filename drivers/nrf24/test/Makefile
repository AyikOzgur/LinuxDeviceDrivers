# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -g

# Target executable
TARGET := nrf24Test

# Source and object files
SRC := main.c
OBJ := $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Link object files into the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Remove build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
