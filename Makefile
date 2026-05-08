CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O0 -g
SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:.c=.o)
TARGET  := mc

.PHONY: all clean test diff_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c src/mc.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@bash test/test.sh

diff_test: $(TARGET)
	@bash test/diff_test.sh

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe tmp.s tmp.out tmp.exe err
