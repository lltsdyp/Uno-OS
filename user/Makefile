# 用来包含testcases.h


include ../common.mk

INCLUDES := ../include

init: initcode.c
	$(CC) $(CFLAGS) -I . -I $(INCLUDES) -march=rv64g -nostdinc -c initcode.c -o initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary initcode.out initcode
	xxd -i initcode > ../include/proc/initcode.h
	rm -f initcode initcode.d initcode.o initcode.out