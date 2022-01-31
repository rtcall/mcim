PREFIX := /usr/local
LDFLAGS := -lcurses
CFLAGS := -Wall -Wmissing-prototypes -O2
CC := cc

all: mcim mcimc

%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

mcim: mcim.o cpu.o
	$(CC) $(LDFLAGS) -o $@ $^ 

mcimc: mcimc.o
	$(CC) -I. -o $@ $^ 

install: mcim mcimc
	mkdir -p $(PREFIX)/bin
	cp $^ $(PREFIX)/bin

clean:
	rm -f mcim mcimc mcim.o cpu.o mcimc.o
