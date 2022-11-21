main : main.o FileSystem.o
	gcc -o main main.o FileSystem.o

main.o : main.c FileSystem.h
	gcc -c main.c

FileSystem.o : FileSystem.c FileSystem.h
	gcc -c FileSystem.c

.PHONY : clean

clean:
		rm main 
		rm myfsys
		rm *.o                                  