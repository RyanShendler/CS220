hamming-decode: hamming-encode
	ln -s -f $< $@

hamming-encode: main.o hamming.o
	gcc main.o hamming.o -o hamming-encode

main.o: main.c hamming.h
	gcc -std=c11 -g -Wall -c main.c

hamming.o: hamming.c hamming.h
	gcc -std=c11 -g -Wall -c hamming.c

clean:
	rm -f *~ *.o hamming-encode hamming-decode

