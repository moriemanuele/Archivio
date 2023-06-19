#include "xerrori.h"

int main(int argc, char **argv) {
  sleep(2);
  if(argc!=2)
    xtermina("Inserisci il nome di un file di testo\n", qui);
  char *nomefile = argv[1];
  //apro il file, se non esiste do errore
  FILE *f = fopen(nomefile,"r");
  if(f==NULL)
    xtermina("Errore apertura file",qui);
 
  int fd_skt = 0;      // file descriptor associato al socket
  struct sockaddr_in serv_addr;
  int e;
  int tmp;

  //ora leggo con la getline e ogni riga che leggo la spedisco con connessione di tipo A, cioè inviando al server una singola sequenza di byte. 
  size_t len = 0;
  
  char *linea = NULL;
 
  
  
  while(1) {
    e = getline(&linea,&len,f);
    int dimensione = e;
    assert(dimensione<Max_sequence_length);
    if(e==-1)
      break;
    
    //spedisco le stringhe con socket
    //vado ad aprire il socket stabilendo una nuova connessione
    if ((fd_skt = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
      termina("Errore creazione socket");
    // assegna indirizzo
    serv_addr.sin_family = AF_INET;
    // il numero della porta deve essere convertito in network order 
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(HOST);
    // apre connessione
    if (connect(fd_skt, &serv_addr, sizeof(serv_addr)) < 0) 
      xtermina("Errore apertura connessione",qui);
    
    //connessione di tipo A: inviano al server una singola sequenza di byte. 

    //ora spedisco 0
    tmp = htonl(0);
    e = write(fd_skt,&tmp,sizeof(tmp));
    if(e!=sizeof(int)) termina("Errore write");

    //ho comunicato che sono tipo di connessione A, quindi procedo ad inviare la stringa
    
    //prima spedisco la dimensione della riga
    int lendecodificata = htonl(dimensione);
    e = writen(fd_skt, &lendecodificata, sizeof(lendecodificata));
    if(e==-1)
      xtermina("Errore invio dimensione della sequenza",qui);
    //ho spedito la dimensione della sequenza, ora spedisco la sequenza stessa
    e = writen(fd_skt,linea,dimensione);
    if(e==-1)
      xtermina("Errore invio sequenza",qui);

    //ho inviato la stringa, chiudo il socket
    if(close(fd_skt)<0)
      xtermina("Errore chiusura socket",qui);
  }
  free(linea);
  if(fclose(f)!=0)
    xtermina("Errore chiusura file",qui);
  return 0;
}
