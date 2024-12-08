.PHONY: clean

build: mkfs.c
	gcc -Werror -Wall -I. -I../include -o mkfs mkfs.c 

clean:
	rm -f mkfs