CFLAGS = `pkg-config --cflags nanomsg json-c`
LIBS = `pkg-config --libs nanomsg json-c`
all: pirouter

pirouter: pirouter.o
	$(CC) -o $@ $(LIBS) $<

install: pirouter
	install pirouter /usr/bin
	install pirouter.service /lib/systemd/system
