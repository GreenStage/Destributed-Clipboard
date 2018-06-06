
CFLAGS = -Wall -ansi -Wextra
LIBS = -lpthread

all: clean
all: CFLAGS += -DVERBOSE
all: clipboard

debug: CFLAGS += -g -O0 -DVERBOSE
debug: clean
debug: clipboard_debug

silent: clean
silent: clipboard

clipboard: clipboard.o clipif.o appif.o clmem.o queue.o rwlock.o packet.o time.o
	gcc -o clipboard clipboard.o appif.o clipif.o clmem.o queue.o rwlock.o packet.o time.o -g $(LIBS) &&\
	rm -rf *.o
	
clipboard_debug: clipboard.o clipif.o appif.o clmem.o queue.o rwlock.o packet.o time.o
	gcc -o clipboard clipboard.o appif.o clipif.o clmem.o queue.o rwlock.o packet.o time.o $(LIBS)

clipboard.o: src/clipboard.c src/interfaces/if.h src/common.h
	gcc -o clipboard.o -c src/clipboard.c $(CFLAGS)

clipif.o: src/interfaces/clipif.c src/interfaces/if.h src/common.h src/mem/clmem.h src/thread/queue.h src/utils/packet.h src/utils/time.h
	gcc -o clipif.o -c src/interfaces/clipif.c $(CFLAGS)

appif.o: src/interfaces/appif.c src/interfaces/if.h src/common.h src/mem/clmem.h src/utils/packet.h src/utils/time.h
	gcc -o appif.o -c src/interfaces/appif.c $(CFLAGS)

clmem.o: src/mem/clmem.c src/mem/clmem.h src/thread/rwlock.h src/common.h
	gcc -o clmem.o -c src/mem/clmem.c $(CFLAGS)

queue.o: src/thread/queue.c src/thread/queue.h src/common.h
	gcc -o queue.o -c src/thread/queue.c $(CFLAGS)

rwlock.o: src/thread/rwlock.c src/thread/rwlock.h
	gcc -o rwlock.o -c src/thread/rwlock.c $(CFLAGS)

packet.o: src/utils/packet.c src/utils/packet.h
	gcc -o packet.o -c src/utils/packet.c $(CFLAGS)

time.o: src/utils/time.c src/utils/time.h src/thread/rwlock.h
	gcc -o time.o -c src/utils/time.c $(CFLAGS)

clean:
	rm -rf *.o ;\
	rm -rf clipboard