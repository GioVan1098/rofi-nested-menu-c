CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11 -I include
CFLAGS  += $(shell pkg-config --cflags json-c)
LDFLAGS := $(shell pkg-config --libs json-c)

TARGET  := rofi-menu
SRCS    := src/main.c src/args.c src/menu.c src/rofi.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all debug clean install

all: CFLAGS += -O2
all: $(TARGET)

# Debug build: no optimisation, ASan + UBSan, debug symbols
debug: CFLAGS  += -g -O0 -DDEBUG -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
