SRCES = scheduler.c shell.c proc-common.c prog.c 
HDRS = proc-common.h request.h
PROGS =  scheduler shell prog

SRCDIR = src
OUTDIR = bin

OBJECTS = $(addprefix $(SRCDIR)/, $(SRCES:.c=.o))
HEADERS = $(addprefix $(SRCDIR)/, $(HDRS))
SOURCES = $(addprefix $(SRCDIR)/, $(SRCES))
PROGRAMS = $(addprefix $(OUTDIR)/, $(PROGS))

CC = gcc
CFLAGS = -Wall -O2 -g 

all: $(PROGRAMS) 

$(OUTDIR)/scheduler: $(OBJECTS)
	$(CC) -o $@ $(SRCDIR)/scheduler.o $(SRCDIR)/proc-common.o

$(OUTDIR)/shell: $(OBJECTS)
	$(CC) -o $@ $(SRCDIR)/shell.o $(SRCDIR)/proc-common.o

$(OUTDIR)/prog: $(OBJECTS)
	$(CC) -o $@ $(SRCDIR)/prog.o $(SRCDIR)/proc-common.o

$(OBJECTS): $(SOURCES) $(HEADERS) 
	$(CC) $(CFLAGS) -o $@ -c $(@:%.o=%.c)

clean:
	rm -f $(OBJECTS) $(PROGRAMS)
