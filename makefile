CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm
TARGET = synthesizer
SRC = main.c midi.c wav.c waves.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
