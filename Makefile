LDLIBS += -lX11 -lXrandr `imlib2-config --libs`
CFLAGS += -std=c99 -Wall -Wextra -O2

.PHONY: all clean

all: swall

clean:
	rm swall
