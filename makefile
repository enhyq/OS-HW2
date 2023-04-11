cimin : cimin.o
	gcc -o cimin cimin.o

cimin.o : cimin.c
	gcc -c -o cimin.o cimin.c

clean : 
	rm *.o cimin 

