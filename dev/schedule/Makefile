ifndef COMPILER
  COMPILER = XL
endif

ifeq ($(COMPILER),GNU)
  CC      = mpicc
  COPT    = -g -Wall -std=gnu99 -O2 -mcpu=a2 -mtune=a2
endif

ifeq ($(COMPILER),XL)
  CC      = mpicxx
  COPT    = -g -O3 #-DDEBUG
endif

LD      = mpixlc_r

OPTIQ = ../..
DEV = $(OPTIQ)/dev
UTIL = $(DEV)/util
STRUCT = $(DEV)/structures
TOPO = $(DEV)/topology
ALG = $(DEV)/algorithms
HEU = $(ALG)/heuristics
YEN  = $(ALG)/yen
TRANS = $(DEV)/transport/pami_transport

CFLAGS  = $(COPT) -I. -I$(UTIL) -I$(STRUCT) -I$(TRANS) -I$(ALG) -I$(HEU) -I$(YEN) -I$(TOPO)
LDFLAGS = $(COPT) -lpthread -lm -L/bgsys/drivers/ppcfloor/bgpm/lib

CFLAGS += -DPROGRESS_THREAD

OBJ =

all: comm_mem.o schedule.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.x: %.o $(OBJ)
	$(LD) $< $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm *.o

realclean: clean
	$(RM) $(RMFLAGS) *.x
	$(RM) $(RMFLAGS) core.*
