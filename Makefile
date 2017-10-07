LDLIBS += -lX11
CFLAGS += -std=c99 -Wall -Wextra -O2

.PHONY: all clean

all: rootfeld

clean:
	rm rootfeld
