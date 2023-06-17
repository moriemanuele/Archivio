# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O -g
LDLIBS = -lm -lrt -pthread


EXECS = archivio client1 client2

all: $(EXECS) 

# regola per la creazioni degli eseguibili utilizzando xerrori.o
$(EXECS): %: %.o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
#	rm -f $^



# regola per la creazione di file oggetto che dipendono da xerrori.h
%.o: %.c xerrori.h
	$(CC) $(CFLAGS) -c $<
 
# regola per rimuovere i file oggetti, gli eseguibili ed i file di log
clean: 
	rm -f *.o $(EXECS)
	rm *.log
