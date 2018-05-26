all: test

test: client_api.o test.o test_gen.o
	gcc -o test test.o client_api.o test_gen.o

test.o: ../Library/clipboard.h test.h test2.c
	gcc -o test.o -c test2.c -g -Wall

test_gen.o: test.h test_gen.c
	gcc -o test_gen.o -c test_gen.c -g -Wall

client_api.o: ../Library/client_api.c ../Library/clipboard.h
	gcc -o client_api.o -c ../Library/client_api.c -g -Wall

clean:
	rm -rf *.o ;\
	rm -rf test