CC = clang 
CFLAGS = -Wall -Wextra
SOURCES = src/rsh.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = rsh

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
