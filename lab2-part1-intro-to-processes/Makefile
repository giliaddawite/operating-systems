CC     = gcc
CFLAGS = -Wall -g
TARGET = my3proc
SRC    = main3.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean