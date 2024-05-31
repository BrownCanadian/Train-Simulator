.PHONY: all clean

all: mts

mts: mts.c
	gcc mts.c -pthread -o mts
   

clean:
	-rm -f mts
