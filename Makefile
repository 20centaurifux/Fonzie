CFLAGS=-DARCH_X86 -DLITTLE_ENDIAN -Os -pedantic -Wall

all:
	gcc -c main.c -o main.o $(CFLAGS)
	gcc -c vm.c -o vm.o -Wall $(CFLAGS)
	gcc main.o vm.o -o vm

clean:
	rm -f *.o
	rm -f *.exe
	rm -f vm
