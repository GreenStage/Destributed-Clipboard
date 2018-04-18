
CFLAGS = -Wall -ansi -Wextra
LIBS = -lpthread

debug: CFLAGS += -g -O0
debug: clean
debug: clipboard

all: clipboard
clipboard: local_clipboard.o dclif.o appif.o clmem.o queue.o rwlock.o
	gcc -o clipboard local_clipboard.o appif.o dclif.o clmem.o queue.o rwlock.o $(LIBS)

local_clipboard.o: src/local_clipboard.c src/interfaces/if.h src/common.h
	gcc -o local_clipboard.o -c src/local_clipboard.c $(CFLAGS)

dclif.o: src/interfaces/dclif.c src/interfaces/if.h src/common.h src/mem/clmem.h src/thread/queue.h
	gcc -o dclif.o -c src/interfaces/dclif.c $(CFLAGS)

appif.o: src/interfaces/appif.c src/interfaces/if.h src/common.h src/mem/clmem.h
	gcc -o appif.o -c src/interfaces/appif.c $(CFLAGS)

clmem.o: src/mem/clmem.c src/mem/clmem.h src/thread/rwlock.h src/common.h
	gcc -o clmem.o -c src/mem/clmem.c $(CFLAGS)

queue.o: src/thread/queue.c src/thread/queue.h src/common.h
	gcc -o queue.o -c src/thread/queue.c $(CFLAGS)

rwlock.o: src/thread/rwlock.c src/thread/rwlock.h
	gcc -o rwlock.o -c src/thread/rwlock.c $(CFLAGS)

clean:
	rm -rf *.o ;\
	rm -rf clipboard