SCRDIR = ./src
BINDIR = ./bin
LIBDIR = ./lib
INCDIR = ./head
OUTDIR = ./output

# _BSD_SOURCE viene usato per nanosleep

CC	=  gcc
INCLUDES  = -I$(INCDIR) 
FLAGS     = -std=c99 -Wall -pedantic -lm -pthread 

CCFLAG = $(INCLUDES) $(FLAGS)


.PHONY : clean test2 result

all: superm

# ------------------------ PER IL SUPERMERCATO -----------------------------
superm : $(BINDIR)/supermarket.o $(BINDIR)/list.o  $(BINDIR)/intlist.o 
	$(CC) $(BINDIR)/supermarket.o $(BINDIR)/list.o $(BINDIR)/intlist.o -o $(OUTDIR)/supermarket  $(FLAGS)

$(BINDIR)/supermarket.o : $(SCRDIR)/supermarket.c
	$(CC) -c $(SCRDIR)/supermarket.c -o $(BINDIR)/supermarket.o $(INCLUDES)

$(BINDIR)/list.o : $(LIBDIR)/list.c 
	$(CC) -c $(LIBDIR)/list.c -o $(BINDIR)/list.o  $(INCLUDES) 

$(BINDIR)/intlist.o : $(LIBDIR)/intlist.c 
	$(CC) -c $(LIBDIR)/intlist.c -o $(BINDIR)/intlist.o  $(INCLUDES) 

# ------------------------ PER IL SUPERMERCATO -----------------------------
test2:
	( $(OUTDIR)/supermarket $(OUTDIR)/config.txt & echo $$! > $(OUTDIR)/procID.PID) & sleep 25s; \
	kill -1 $$(cat $(OUTDIR)/procID.PID);

result : 
	./analisi.sh;

clean:
	rm $(BINDIR)/*.o; \
	rm $(OUTDIR)/supermarket; \
	rm $(OUTDIR)/logfile.txt; \
	rm $(OUTDIR)/procID.PID ; \
