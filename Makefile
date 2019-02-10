CC=powerpc-eabi-gcc
CFLAGS=-std=gnu99 -nostdinc -fno-builtin -c
LD=powerpc-eabi-ld
LDFLAGS=-Ttext 1800000 -Tdata 20000000 --oformat binary
project	:=	src
override CURDIR:=$(shell cygpath -m $(CURDIR))
root:=$(CURDIR)
build	:=	 $(root)/bin
libs := $(root)/../../libwiiu/bin
www :=$(root)/../../www
framework:=$(root)/../../framework
all: setup main550 
setup:
	mkdir -p $(root)/bin/
main550:
	$(CC) $(CFLAGS) -DVER=550 $(project)/*.c
	#-Wa,-a,-ad
	cp -r $(root)/*.o $(build)
	rm $(root)/*.o
	$(LD) $(LDFLAGS) -o $(build)/code550.bin $(build)/*.o $(libs)/550/*.o `find $(build) -name "*.o" ! -name "loader.o"`
clean:
	rm -r $(build)/*
