OS = $(shell uname -s)
#OS = Android

all: gc

CFLAGS = -Wno-deprecated-declarations -pthread

#NOCL = 1
SRC = gcv.c

ifneq (${NOCL},1)
    SRC += cl.c
ifeq (${OS},Darwin)
    CFLAGS += -framework OpenCL
else ifeq (${OS},Linux)
    CFLAGS += -lOpenCL
else ifeq (${OS},Android)
	CFLAGS += -L/system/vendor/lib64 -lOpenCL -I./opencl-headers
endif
else
    CFLAGS += -DNOCL
endif

gc:
	gcc ${SRC} ${CFLAGS}

.PHONY: clean

clean:
	rm -f *.o
	rm -f gcv
