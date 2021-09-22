CC = gcc
CFLAGS = -std=c11 -g -Wall
CPPFLAGS = -I $$HOME/cs220/include
LDFLAGS = -L $$HOME/cs220/lib -l cs220 -l y86

stall-sim: main.o stall-sim.o
	$(CC) main.o stall-sim.o $(LDFLAGS) -o $@

main.o: main.c stall-sim.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

stall-sim.o: stall-sim.c stall-sim.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm *.o stall-sim
