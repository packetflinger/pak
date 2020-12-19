all:
	gcc -g --std=c99 -o pak pak.c

clean:
	rm -f pak
