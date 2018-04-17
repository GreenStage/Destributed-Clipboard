all: test

test: client_api.o test.o
	gcc -o test test.o client_api.o

test.o: clipboard.h test.c
	gcc -o test.o -c test.c -g -Wall

client_api.o: client_api.c clipboard.h
	gcc -o client_api.o -c client_api.c -g -Wall

clean:
	rm -rf *.o ;\
	rm -rf test