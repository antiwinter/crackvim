OS = $(shell uname -s)

all: gc

CFLAGS = -Wno-deprecated-declarations -pthread

ifeq (${OS},Darwin)
    CFLAGS += -framework OpenCL
else ifeq (${OS},Linux)
    CFLAGS += -lOpenCL
endif

gc:
	gcc gcv.c cl.c ${CFLAGS}

.PHONY: clean

clean:
	rm -f *.o
	rm -f gcv
