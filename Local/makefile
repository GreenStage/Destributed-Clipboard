
CFLAGS = -Wall -ansi -Wextra
LIBS = -lpthread

debug: CFLAGS += -g -O0
debug: clean
debug: clipboard

all: clipboard
clipboard: local_clipboard.o dclif.o appif.o clmem.o queue.o time_m.o
	gcc -o clipboard local_clipboard.o appif.o dclif.o clmem.o queue.o time_m.o $(LIBS)

local_clipboard.o: local_clipboard.c clmem.h if.h common.h
	gcc -o local_clipboard.o -c local_clipboard.c $(CFLAGS)

dclif.o: dclif.c if.h common.h clmem.h utils/queue.h utils/time_m.h
	gcc -o dclif.o -c dclif.c $(CFLAGS)

appif.o: appif.c if.h common.h clmem.h utils/time_m.h
	gcc -o appif.o -c appif.c $(CFLAGS)

clmem.o: clmem.c clmem.h common.h  utils/time_m.h
	gcc -o clmem.o -c clmem.c $(CFLAGS)

queue.o: utils/queue.c utils/queue.h
	gcc -o queue.o -c utils/queue.c $(CFLAGS)

time_m.o: utils/time_m.c utils/time_m.h
	gcc -o time_m.o -c utils/time_m.c $(CFLAGS)

clean:
	rm -rf *.o ;\
	rm -rf clipboard