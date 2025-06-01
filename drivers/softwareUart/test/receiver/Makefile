CC = gcc
CFLAGS = -Wall -Wextra

TARGET = receiverTest
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)