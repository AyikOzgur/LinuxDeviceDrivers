CC = gcc
CFLAGS = -Wall -Wextra

TARGET = transmitterTest
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)