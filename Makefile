CFLAGS = `pkg-config --cflags nanomsg json-c`
CXXFLAGS = `pkg-config --cflags nanomsg gtkmm-3.0`
LIBS = `pkg-config --libs nanomsg json-c`

all: pirouter piviewer

pirouter: pirouter.o
	$(CC) -o $@ $(LIBS) $<

piviewer: piviewer.o
	g++ -o piviewer `pkg-config --libs nanomsg libtsm gtkmm-3.0` piviewer.o

install: pirouter
	install pirouter /usr/bin
	install piviewer /usr/bin
	install pirouter.service /lib/systemd/system

clean:
	rm -rf pirouter piviewer *.o
