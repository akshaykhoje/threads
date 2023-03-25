all: 
	gcc -Wall threads.c -lpthread
clean:
	rm a.out