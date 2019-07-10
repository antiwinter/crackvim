all: crackvim zip zfp gc


pkzip_crypto.o: pkzip_crypto.c pkzip_crypto.h
	gcc -c -o $@ $<

crackvim.o: crackvim.c crc32.h pkzip_crypto.h
	gcc -c -o $@ $<

crc32.o: crc32.c
	gcc -c -o $@ $<

crackvim: crackvim.o crc32.o pkzip_crypto.o
	gcc -pthread -o $@ $^

zip:
	g++ -std=c++11 zipforce.cpp

gc:
	gcc -framework OpenCL -Wno-deprecated-declarations gcv.c cl.c

zfp:
	g++ -std=c++11 -framework OpenCL -Wno-deprecated-declarations zfg.cpp 

.PHONY: clean

clean:
	rm -f *.o
	rm -f crackvim
