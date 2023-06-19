#include "xerrori.h"


void *tbody(void *argv) {
  //per prima cosa apro il file
  FILE *f = fopen((char *)argv,"r");
  if(f==NULL)
    xtermina("Errore apertura file", qui);
  
  //in secondo luogo apro la connessione
  int fd_skt = 0;      // file descriptor associato al socket
  struct sockaddr_in serv_addr;
  int e;
  int tmp;
  // crea socket
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
  //comunico che sono una connessione di tipo B: ora spedisco 1
  tmp = htonl(1);
  e = write(fd_skt,&tmp,sizeof(tmp));
  if(e!=sizeof(int)) termina("Errore write");
  int numerosequenzespedite = 0;
  //ho comunicato he sono una connessione di tipo B, procedo a inviare una alla volta le linee del file, cioè un numero imprecisato di sequenze di byte; quando il client ha inviato l'ultima sequenza esso segnala che non ce ne sono altre inviandone una di lunghezza 0. successivamente devo ricevere un intero  che indica il numero totale di sequenze che il server ha ricevuto durante la sessione.
  size_t len = 0;
  char *linea = NULL;
  while(1) {
    e = getline(&linea,&len,f);
    int dimensione = e;
    if(e==-1)
      break;
    assert(dimensione<Max_sequence_length);
    //spedisco la dimensione della sequenza
    int lendecodificata = htonl(dimensione);
    e = writen(fd_skt,&lendecodificata,sizeof(lendecodificata));
    if(e==-1)
      xtermina("Errore invio dimensione della sequenza",qui);
    e = writen(fd_skt,linea,dimensione);
    if(e==-1)
      xtermina("Errore invio sequenza",qui);
    numerosequenzespedite++;
  }
  //ho finito le stringhe da spedire, spedisco una stringa di lunghezza 0
  int zerodecodificato = htonl(0);
  e = writen(fd_skt,&zerodecodificato,sizeof(zerodecodificato));
  if(e==-1)
    xtermina("Errore invio 0 prima dell'ultima sequenza",qui);
  
  e = writen(fd_skt,"",0);
  if(e==-1)
    xtermina("Errore invio sequenza",qui);
  //mi metto in attesa per ricevere un intero
  int numerosequenzedadecodificare;
  e = readn(fd_skt, &numerosequenzedadecodificare, sizeof(numerosequenzedadecodificare));
  int numerosequenze = ntohl(numerosequenzedadecodificare);
  if(numerosequenze!=numerosequenzespedite)
    xtermina("Il numero di sequenze spedite da client2 è diverso dal numero di sequenze ricevute dal server\n",qui);
  if(close(fd_skt)<0)
    xtermina("Errore chiusura socket",qui);
  free(linea);
  if(fclose(f)!=0)
    xtermina("Errore chiusura file",qui);
  pthread_exit(NULL);
}



int main(int argc, char **argv) {
  sleep(1);
  if(argc<2)
    xtermina("Inserisci uno o più file di testo da cui leggere",qui);
  pthread_t client2[argc-1];
  for(int i = 0;i<argc-1;i++) {
    char *nomefile = argv[i+1];
    xpthread_create(&client2[i],NULL, tbody, nomefile, qui);
  }

  for(int i =0;i<argc-1;i++)
    xpthread_join(client2[i], NULL, qui);
  return 0;
}
