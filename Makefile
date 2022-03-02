all: servidor cliente

servidor: bin/servidor

cliente: bin/cliente

bin/servidor: obj/servidor.o
	gcc -g obj/servidor.o -o bin/servidor

obj/servidor.o: src/servidor.c
	gcc -Wall -g -c src/servidor.c -o obj/servidor.o

bin/cliente: obj/cliente.o
	gcc -g obj/cliente.o -o bin/cliente

obj/cliente.o: src/cliente.c
	gcc -Wall -g -c src/cliente.c -o obj/cliente.o

clean:
	rm obj/* tmp/* bin/servidor bin/cliente *.mp3

test:
	bin/cliente transform samples/sample-1-so.m4a tmp/sample-1.mp3 alto eco
	bin/cliente transform samples/sample-2-miei.m4a tmp/sample-2.mp3 lento baixo
