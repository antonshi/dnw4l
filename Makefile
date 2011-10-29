#!/usr/bin/make
ifndef ENV
ENV	= -m32
endif

CFLAGS	= $(ENV) -Wall -O
LDFLAGS	= $(ENV)
LIBS	= -lusb-1.0
APP	= dnw4l
OBJ	= dnw4l.o

.PHONY: all
all: $(APP)

$(APP): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f *.o $(APP)
