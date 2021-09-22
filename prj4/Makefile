CC = gcc
CFLAGS = -std=c11 -g -Wall
COURSE = cs220
TARGET = y86-sim
OBJS = main.o ysim.o
CPPFLAGS = -I $$HOME/$(COURSE)/include
LDFLAGS = -L $$HOME/$(COURSE)/lib -l cs220 -l y86

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

main.o: main.c ysim.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

ysim.o: ysim.c ysim.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm *.o y86-sim
