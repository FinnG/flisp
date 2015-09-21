MPC_DIR=/home/gfg/Personal/git/mpc

CC=gcc
CFLAGS=-g
INCLUDES=-I $(MPC_DIR)
LIBS=-ledit -lm
SRCS=main.c $(MPC_DIR)/mpc.c


.PHONY: all

all: flisp

flisp: $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS) $(LIBS)
