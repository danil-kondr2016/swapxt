.PHONY: all clean

all: swapxt swapyt

swapxt: swapxt.o
	cc swapxt.o -lpng -o swapxt

swapyt: swapyt.o
	cc swapyt.o -lpng -o swapyt

.c.o:
	cc -c $<

clean:
	rm *.o swapxt swapyt
