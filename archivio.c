#include "xerrori.h"

#define Num_elem 1000000
#define PC_buffer_len 10


typedef struct {
  int valore; // numero di occorrenze della stringa
  ENTRY *next;
} coppia;


void stampa_numero_entry(int n, int fd) {
  ssize_t e=0;
  int i=0;
  if(n<10) {
    char buffer[] = "Numero entry: X\n";
    buffer[14] = '0'+n;
    e = write(fd,buffer,16);
  }
  else if(n<100) {
    char buffer[] = "Numero entry: XX\n";
    buffer[14] = '0'+ n/10;
    buffer[15] = '0' + n%10;
    e = write(fd,buffer,17);
  }
  else if(n<1000) {
    char buffer[] = "Numero entry: XXX\n";
    buffer[14] = '0'+ n/100;
  //caso 987
    int tmp = n%100; //87
    buffer[15] = '0' + tmp/10;
    buffer[16] = '0' + n%10;
    e = write(fd,buffer,18);
  }
  else if(n<10000) {
    char buffer[] = "Numero entry: XXXX\n";
    while(i<4) {
      buffer[14+i] = '0'+ (n / (int) pow(10,3-i))%10;
      i++;
    }
    e = write(fd,buffer,19); 
  }
  else if(n<100000) {
    char buffer[] = "Numero entry: XXXXX\n";
      while(i<5) {
        buffer[14+i] = '0'+ (n / (int) pow(10,4-i))%10;
        i++;
      }
    e = write(fd,buffer,20); 
  }
  else if(n<1000000) {
    char buffer[] = "Numero entry: XXXXXX\n";
      while(i<6) {
        buffer[14+i] = '0'+ (n / (int) pow(10,5-i))%10;
        i++;
      }
    e = write(fd,buffer,21);
  }
  else if (n>=1000000)
    xtermina("Numero entry registrate maggiore del numero di entry possibile",qui);
  if(e<16)
    xtermina("Errore write",qui);
}


typedef struct {
  int *ccindex;
  pthread_mutex_t *mutexhashtable;
  pthread_mutex_t *mutexbufferw;
  char **bufferw;
  pthread_cond_t *readgo; 
  pthread_cond_t *writego;  
  pthread_cond_t *emptyw;
  pthread_cond_t *fullw;
  int *datiwcapow;
  int *datirw;
  int *waitingwriters;
  int *activereaders;
  int *activewriters;
  int *waitingreaders;
  int *numeroentry;
  ENTRY **testa_lista_entry;
} writer;

typedef struct {
  pthread_mutex_t *mutexbufferw;
  pthread_cond_t *emptyw; 
  pthread_cond_t *fullw; 
  char **bufferw;
  int *datiwcapow;
  int numw;
  pthread_t * arrayscrittori;
} capowriter;

typedef struct {
  int *ccindex;
  pthread_mutex_t *mutexhashtable;
  pthread_mutex_t *mutexbufferr;
  pthread_mutex_t *mutexfile;
  char **bufferr;
  pthread_cond_t *readgo;  
  pthread_cond_t *writego;   
  pthread_cond_t *emptyr; 
  pthread_cond_t *fullr;  
  int *datirw;
  int *datircapor;
  int *waitingwriters;
  int *activereaders;
  int *activewriters;
  int *waitingreaders;
  FILE *f;
} reader;

typedef struct {
  pthread_mutex_t *mutexbufferr;
  char **bufferr;
  pthread_cond_t *emptyr; 
  pthread_cond_t *fullr;  
  int *datircapor;
  int numr;
  pthread_t * arraylettori;
  FILE *f;
} caporeader;

typedef struct {
  pthread_t *caporeader;
  pthread_t *capowriter;
  pthread_mutex_t *mutexhashtable;
  pthread_cond_t *readgo;  
  pthread_cond_t *writego;
  int *waitingwriters;
  int *activereaders;
  int *activewriters;
  int *waitingreaders;
  int *hashtable;
  int *numeroentry;
  ENTRY **testa_lista_entry;
} manager;

void acquisisci_accesso_scrittura_buffer(pthread_mutex_t * mutexbuffer, int * dati, pthread_cond_t * full) {
  xpthread_mutex_lock(mutexbuffer,qui);
  while(*(dati)==PC_buffer_len) { //aspetto finché il buffer non è più pieno
    xpthread_cond_wait(full,mutexbuffer,qui); 
  }
}

void rilascia_accesso_scrittura_buffer(pthread_mutex_t * mutexbuffer, int * dati, pthread_cond_t * empty) {
  *(dati) += 1;
  xpthread_cond_signal(empty,qui);
  xpthread_mutex_unlock(mutexbuffer,qui);
}

void acquisisci_accesso_lettura_buffer(pthread_mutex_t * mutexbuffer, int * dati, pthread_cond_t * empty) {
  xpthread_mutex_lock(mutexbuffer,qui);
  while(*(dati)==0) {
    // attende fino a quando il buffer è vuoto
    //se il buffer è vuoto aspetto su emptyw 
    xpthread_cond_wait(empty,mutexbuffer,qui);
  }
}

void rilascia_accesso_lettura_buffer(pthread_mutex_t * mutexbuffer, int * dati, pthread_cond_t * full) {
  *(dati) -= 1;
  // segnala che il buffer non è più pieno
  xpthread_cond_signal(full,qui);
  xpthread_mutex_unlock(mutexbuffer,qui);
}

void distruggi_entry(ENTRY *e) {
  free(e->key);
  free(e->data);
  free(e);
}

void distruggi_lista(ENTRY *head) {
  if(head!=NULL) {
    distruggi_lista(((coppia *) head->data)->next); 
    distruggi_entry(head);//free(head->key); free(head->data); free(head);
  }
}

ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY)); 
  if (e == NULL)
    xtermina("Errore malloc", qui);
  e->key = strdup(s); // salva copia di s
  e->data = malloc(sizeof(coppia));
  if (e->key == NULL || e->data == NULL)
    xtermina("Errore malloc", qui);
  coppia *c = (coppia *)e->data;
  c->valore = n;
  c->next = NULL;
  return e;
}

void aggiungi(char *s, int *numeroentry, ENTRY ** testa_lista_entry) {
  ENTRY *e = crea_entry(s, 1);
  ENTRY *r = hsearch(*e, FIND);
  //static int numeroentry = 0;
  if (r == NULL) {          // la entry è nuova
    r = hsearch(*e, ENTER); // r: ENTRY *
    if (r == NULL)
      xtermina("Errore o tabella piena", qui);
    coppia *c = (coppia *) e->data;
    c->next = *(testa_lista_entry);
    *(testa_lista_entry) = e;
    *(numeroentry)+=1;
  
  } 
  else {
    assert(strcmp(e->key, r->key) == 0);
    coppia *c = (coppia *)r->data;
    c->valore += 1;
    distruggi_entry(e); // questa non la devo memorizzare
  }
}

int conta(char *s) {
  //S VA DEALLOCATA
  ENTRY *e = crea_entry(s, 1);
  ENTRY *r = hsearch(*e, FIND);

  if(r==NULL) { //non c'è nessuna chiave di nome s
    distruggi_entry(e);
    return 0;  
  }
  
  coppia *c = (coppia *) r->data;
  int d = c->valore;
  distruggi_entry(e);
  return d;
}



void * capowritersbody(void *argv) {
  capowriter * a = (capowriter *) argv;
  char *sep =".,:; \n\r\t";
  //apro la pipe in lettura
  int fd = open("caposc",O_RDONLY);
  if(fd<0) 
    xtermina("Errore apertura named pipe scrittore",qui);
  int ppindex = 0;
  while(true) {
    //nella lettura devo tener conto dell'endianess
    int lunghezzadadecodificare;
    ssize_t e;
    e = read(fd,&lunghezzadadecodificare,sizeof(lunghezzadadecodificare)); //leggo quanti byte mi stanno per arrivare
    if(e==0) break;
    int lunghezza = ntohl(lunghezzadadecodificare);
    char *s= malloc(sizeof(char)*(lunghezza + 1)); 
    if(s==NULL)
      xtermina("Errore malloc", qui);
    e = read(fd,s,lunghezza*sizeof(char)); //leggo tanti byte quanti me ne sono stati detti poc'anzi
    assert(e!=0);
    s[lunghezza] = '\0';
    //effettuo la tokenizzazione
    char *token;
    char * deallocami = s;
    while ((token = strtok_r(s, sep, &s))) {
      //token è la stringa delimitata. inserisco token dentro il buffer in mutua esclusione
      acquisisci_accesso_scrittura_buffer(a->mutexbufferw,a->datiwcapow,a->fullw);
      char *inseriscimi = strdup(token); 
      a->bufferw[(ppindex ++) %PC_buffer_len] = inseriscimi;
      rilascia_accesso_scrittura_buffer(a->mutexbufferw,a->datiwcapow,a->emptyw);
    }
    free(deallocami); 
  }
  //chiudo la pipe in lettura
  xclose(fd,qui);
  //sono uscito dal while, quindi comunico numw volte che non ho più nessun valore da mettere nel buffer w capow
  for(int i = 0;i< a->numw ;i++) {
      acquisisci_accesso_scrittura_buffer(a->mutexbufferw,a->datiwcapow,a->fullw);
      a->bufferw[(ppindex ++) %PC_buffer_len] =NULL;
      rilascia_accesso_scrittura_buffer(a->mutexbufferw,a->datiwcapow,a->emptyw);
  }
  //ho comunicato a tutti gli scrittori che devono terminare, dunque attendo la loro terminazione
  for(int i = 0;i<a->numw;i++) {
    xpthread_join(a->arrayscrittori[i], NULL,qui);
  }
  return NULL;
}

void * writersbody(void *argv) {
  writer *a = (writer *) argv;

  //devo leggere in mutua esclusione dal bufferw-capow
  while(1) {
    char *s;
    acquisisci_accesso_lettura_buffer(a->mutexbufferw,a->datiwcapow,a->emptyw);
    s = a->bufferw[*(a->ccindex) % PC_buffer_len];
    *(a->ccindex) += 1;
    rilascia_accesso_lettura_buffer(a->mutexbufferw,a->datiwcapow,a->fullw);
    if(s==NULL) //il caposcrittore ha messo null sul buffer per segnalare di aver finito, quindi esco dal ciclo e valuterò come 
      break; 
    //ho letto il dato dal buffer in mutua esclusione dal buffer w-capow, lo inserisco nella hashtable

    xpthread_mutex_lock(a->mutexhashtable, qui);
    *(a->waitingwriters)+=1;
    while(*(a->activewriters)>0 || *(a->activereaders)>0)
      xpthread_cond_wait(a->writego, a->mutexhashtable, qui);
    *(a->waitingwriters)-=1; 
    *(a->activewriters)+=1; 
    xpthread_mutex_unlock(a->mutexhashtable, qui);

    aggiungi(s,a->numeroentry,a->testa_lista_entry);
    
    xpthread_mutex_lock(a->mutexhashtable, qui);
    *(a->activewriters)-=1;
    if(*(a->waitingreaders)>0)
      xpthread_cond_broadcast(a->readgo,qui);
    else
      xpthread_cond_signal(a->writego, qui);
    xpthread_mutex_unlock(a->mutexhashtable, qui);
    free(s); 
  }
  return NULL;
}


void * caporeaderbody(void *argv) {
  caporeader * a = (caporeader *) argv;
  //apro la pipe in lettura
  int fd = open("capolet",O_RDONLY);
  if(fd<0)
    xtermina("Errore apertura named pipe lettore", qui);
  int ppindex = 0;
  char *sep =".,:; \n\r\t";
  while(1) {
    int lunghezzadadecodificare;
    ssize_t e;
    e = read(fd,&lunghezzadadecodificare,sizeof(lunghezzadadecodificare)); //leggo quanti byte mi stanno per arrivare
    if(e==0) break;
    int lunghezza = ntohl(lunghezzadadecodificare);
    char *s = malloc(sizeof(char)*(lunghezza+1)); 
    if(s==NULL)
      xtermina("Errore malloc", qui);
    e = read(fd,s,lunghezza*sizeof(char)); //leggo tanti byte quanti me ne sono stati detti poc'anzi
    assert(e!=0);
    s[lunghezza] = '\0';
    
    char *deallocami = s;
    char *token;
    while ((token = strtok_r(s, sep, &s))) {
      acquisisci_accesso_scrittura_buffer(a->mutexbufferr,a->datircapor,a->fullr);
      char *inseriscimi = strdup(token); 
      a->bufferr[(ppindex++) %PC_buffer_len] = inseriscimi;
      rilascia_accesso_scrittura_buffer(a->mutexbufferr,a->datircapor,a->emptyr);
    }
    free(deallocami);
  }
  //chiudo la pipe in lettura
  xclose(fd,qui);
  //sono uscito dal while, quindi comunico numr volte che non ho più nessun valore da mettere nel buffer r capor
  for(int i = 0;i< a->numr;i++) {
      acquisisci_accesso_scrittura_buffer(a->mutexbufferr,a->datircapor,a->fullr);
      a->bufferr[(ppindex++) %PC_buffer_len] =NULL;
      rilascia_accesso_scrittura_buffer(a->mutexbufferr,a->datircapor,a->emptyr);
  }
  //ho comunicato a tutti i lettori che devono terminare, dunque attendo la loro terminazione
  for(int i = 0;i< a->numr ;i++)
    xpthread_join(a->arraylettori[i], NULL,qui);
  
  //chiudo lettori.log
  if(fclose(a->f)!=0)
    xtermina("Errore chiusura file",qui);
  
  return NULL;
}

void * readersbody(void *argv) {
  reader * a = (reader *) argv;
  //prima devo leggere dal buffer e poi dalla tabella hash la parola che ho appena letto
  while(1) {
    char *s;
    acquisisci_accesso_lettura_buffer(a->mutexbufferr,a->datircapor,a->emptyr);
    s = a->bufferr[*(a->ccindex) % PC_buffer_len];
    *(a->ccindex) += 1;
    rilascia_accesso_lettura_buffer(a->mutexbufferr,a->datircapor,a->fullr);

    if(s==NULL) //il capolettore ha messo null sul buffer per segnalare di aver finito, quindi esco dal ciclo e valuterò come 
      break; 

    //ho letto la stringa  dal buffer, procedo a cercarla sulla tabella hash
    xpthread_mutex_lock(a->mutexhashtable, qui);
    *(a->waitingreaders)+=1;
    while(*(a->activewriters)>0) 
      xpthread_cond_wait(a->readgo, a->mutexhashtable, qui);
    *(a->waitingreaders)-=1;
    *(a->activereaders)+=1;
    xpthread_mutex_unlock(a->mutexhashtable, qui);
    
    int n = conta(s);

    xpthread_mutex_lock(a->mutexhashtable, qui);
    *(a->activereaders)-=1;
    if(*(a->activereaders)==0 && *(a->waitingwriters)>0)
      xpthread_cond_signal(a->writego, qui);
    xpthread_mutex_unlock(a->mutexhashtable, qui);

    //scrivo stringa e numero nel file in mutua esclusione
    xpthread_mutex_lock(a->mutexfile, qui);
    fprintf(a->f, "%s %d\n",s,n);
    xpthread_mutex_unlock(a->mutexfile, qui);
    free(s);
  }
  return NULL;
}


void * gestorebody(void *argv) {
  manager *a = (manager *) argv;
  //mi metto in attesa di tutti i segnali
  sigset_t mask;
  sigfillset(&mask);
  int s;
  volatile sig_atomic_t continua = 1;
  while(continua==1) {
    int e = sigwait(&mask,&s);
    if(e!=0)
      xtermina("Errore sigwait",qui);
    switch (s) {
      case SIGINT:
        //blocco tutti i segnali per evitare di andare in deadlock
        pthread_sigmask(SIG_BLOCK,&mask,NULL);
        //devo leggere la variabile globale in mutua esclusione perché sennò potrei leggerla mentre 
        //viene modificata da uno scrittore
        xpthread_mutex_lock(a->mutexhashtable, qui);
        *(a->waitingreaders)+=1;
        while(*(a->activewriters)>0) 
          xpthread_cond_wait(a->readgo, a->mutexhashtable, qui);
        *(a->waitingreaders)-=1;
        *(a->activereaders)+=1;
        xpthread_mutex_unlock(a->mutexhashtable, qui);
        //stampo il numero di entry
        stampa_numero_entry(*(a->numeroentry),2);
        xpthread_mutex_lock(a->mutexhashtable, qui);
        *(a->activereaders)-=1;
        if(*(a->activereaders)==0 && *(a->waitingwriters)>0)
          xpthread_cond_signal(a->writego, qui);
        xpthread_mutex_unlock(a->mutexhashtable, qui);
        //sblocco tutti i segnali per poterne ricevere altri
        pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        break;
      case SIGTERM:
        //blocco tutti i segnali per evitare di essere interrotto
        pthread_sigmask(SIG_BLOCK,&mask,NULL);
        //attendo la terminazione dei capi
        xpthread_join(*(a->caporeader), NULL,qui);
        xpthread_join(*(a->capowriter), NULL,qui);
        //stampo su stdout il numero totale di stringhe contenute dentro la tabella hash
        //non ho bisogno di mutua esclusione perché hanno già terminato anche tutti gli scrittori e tutti i lettori
        stampa_numero_entry(*(a->numeroentry),1);
        distruggi_lista(*(a->testa_lista_entry)); 
        hdestroy();
        continua = 0;
        break;
      case SIGUSR1:
        //blocco tutti i segnali per evitare di andare in deadlock
        pthread_sigmask(SIG_BLOCK,&mask,NULL);
        xpthread_mutex_lock(a->mutexhashtable, qui);
        *(a->waitingwriters)+=1;
        while(*(a->activewriters)>0 || *(a->activereaders)>0)
          xpthread_cond_wait(a->writego, a->mutexhashtable, qui);
        *(a->waitingwriters)-=1;
        *(a->activewriters)+=1;
        xpthread_mutex_unlock(a->mutexhashtable, qui);
        //dealloco tutto
        distruggi_lista(*(a->testa_lista_entry));
        hdestroy();
        *(a->numeroentry)=0;
        *(a->testa_lista_entry) = NULL; //forse mi leverai!!!
        *(a->hashtable) = hcreate(Num_elem);
        xpthread_mutex_lock(a->mutexhashtable, qui);
        *(a->activewriters)-=1;
        if(*(a->waitingreaders)>0)
          xpthread_cond_broadcast(a->readgo,qui);
        else
          xpthread_cond_signal(a->writego, qui);
        xpthread_mutex_unlock(a->mutexhashtable, qui);
        //sblocco tutti i segnali per riceverne altri
        pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        break;
    }
  }
  return NULL;
}

int main(int argc, char **argv) {
  if (argc != 3)
    xtermina("Inserisci il numero di lettori ed il numero di scrittori\n", qui);
  int numw = atoi(argv[1]);
  int numr = atoi(argv[2]);

  if(numw<=0 || numr<=0)
    xtermina("I numer di thread lettori e di thread scrittori devono essere entrambi non negativi",qui);

  int hashtable = hcreate(Num_elem);
  if (hashtable == 0)
    xtermina("Errore creazione tabella hash", qui);

  //inizializzo il contatore del numero di ENTRY e la rispettiva lista
  ENTRY *testa_lista_entry = NULL;
  int numeroentry = 0;
  // inizializzo i mutex e le cv
  // ho bisogno di mutex per bufferw-cw, bufferr-cr,bufferrw
  pthread_mutex_t mutexbufferr = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexhashtable = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexbufferw = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexfile = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t readgo = PTHREAD_COND_INITIALIZER; 
  pthread_cond_t writego = PTHREAD_COND_INITIALIZER;  
  pthread_cond_t emptyr = PTHREAD_COND_INITIALIZER;
  pthread_cond_t fullr = PTHREAD_COND_INITIALIZER;
  pthread_cond_t emptyw = PTHREAD_COND_INITIALIZER;
  pthread_cond_t fullw = PTHREAD_COND_INITIALIZER;
  char *bufferw[PC_buffer_len]; // buffer tra scrittori e capo è array di stringhe
  char *bufferr[PC_buffer_len]; // buffer tra lettori e capo è array di stringhe
  int datirw = 0;
  int datiwcapow = 0;
  int datircapor = 0;
  int waitingwriters = 0;
  int activereaders = 0;
  int activewriters = 0;
  int waitingreaders = 0;

  
  // blocco tutti i segnali
  sigset_t mask;
  sigfillset(&mask); //riempo la maschera di tutti i segnali
  pthread_sigmask(SIG_BLOCK,&mask,NULL); // blocco tutto 
  // faccio partire il thread gestore
  pthread_t gestore;
  pthread_t capolettore;
  pthread_t caposcrittore;
  pthread_t readers[numr];
  pthread_t writers[numw];
  manager datig;
  datig.caporeader = &capolettore;
  datig.capowriter = &caposcrittore;
  datig.mutexhashtable = &mutexhashtable;
  datig.readgo = &readgo;
  datig.writego = &writego;
  datig.activereaders = &activereaders;
  datig.activewriters = &activewriters;
  datig.waitingreaders = &waitingreaders;
  datig.waitingwriters = &waitingwriters;
  datig.hashtable = &hashtable;
  datig.numeroentry = &numeroentry;
  datig.testa_lista_entry = &testa_lista_entry;
  xpthread_create(&gestore, NULL, &gestorebody, &datig, qui);


  // faccio partire il thread capo lettore
  FILE *flettori = fopen("lettori.log", "w");
  if(flettori==NULL)
    xtermina("Errore apertura lettori.log in scrittura", qui);
  caporeader dr;
  dr.mutexbufferr = &mutexbufferr;
  dr.bufferr = bufferr;
  dr.emptyr = &emptyr;
  dr.fullr = &fullr;
  dr.datircapor = &datircapor;
  dr.numr = numr;
  dr.arraylettori = readers;
  dr.f = flettori;
  xpthread_create(&capolettore, NULL, &caporeaderbody, &dr, qui);
  // faccio partire i lettori
  reader datareader[numr];
  int cindexr = 0;
  
  for (int i = 0; i < numr; i++) {
    datareader[i].mutexbufferr = &mutexbufferr; 
    datareader[i].ccindex = &cindexr; 
    datareader[i].bufferr = bufferr; 
    datareader[i].mutexhashtable = &mutexhashtable; 
    datareader[i].mutexfile = &mutexfile; 
    datareader[i].emptyr = &emptyr;
    datareader[i].readgo = &readgo;
    datareader[i].writego = &writego;
    datareader[i].fullr = &fullr;
    datareader[i].datircapor = &datircapor;
    datareader[i].datirw = &datirw;
    datareader[i].activereaders = &activereaders;
    datareader[i].activewriters = &activewriters;
    datareader[i].waitingreaders = &waitingreaders;
    datareader[i].waitingwriters = &waitingwriters;
    datareader[i].f = flettori;
    
    xpthread_create(&readers[i], NULL, &readersbody, datareader + i, __LINE__, __FILE__);
  }
  
  //faccio partire il capo scrittore
  capowriter dw;
  dw.mutexbufferw = &mutexbufferw;
  dw.bufferw = bufferw;
  dw.emptyw = &emptyw;
  dw.fullw = &fullw;
  dw.datiwcapow = &datiwcapow;
  dw.numw = numw;
  dw.arrayscrittori = writers;
 
  xpthread_create(&caposcrittore, NULL, &capowritersbody, &dw, qui);
  
  // faccio partire gli scrittori
  writer datawriter[numw];
  int cindexw = 0;
  for (int i = 0; i < numw; i++) {
    datawriter[i].mutexbufferw = &mutexbufferw; 
    datawriter[i].ccindex = &cindexw;
    datawriter[i].bufferw = bufferw;
    datawriter[i].mutexhashtable = &mutexhashtable;
    datawriter[i].readgo = &readgo;
    datawriter[i].writego = &writego;
    datawriter[i].emptyw = &emptyw;
    datawriter[i].fullw = &fullw;
    datawriter[i].datirw = &datirw;
    datawriter[i].datiwcapow = &datiwcapow;
    datawriter[i].activereaders = &activereaders;
    datawriter[i].activewriters = &activewriters;
    datawriter[i].waitingreaders = &waitingreaders;
    datawriter[i].waitingwriters = &waitingwriters;
    datawriter[i].numeroentry = &numeroentry;
    datawriter[i].testa_lista_entry = &testa_lista_entry;
    xpthread_create(&writers[i], NULL, &writersbody, datawriter + i, __LINE__, __FILE__);
  }
  
  xpthread_join(gestore, NULL, qui);
  //ha terminato il gestore che ha atteso la terminazione dei caposcrittore e capolettori, 
  //i quali rispettivamente hanno atteso la terminazione di scrittori e lettori.
  //dunque archivio può terminare
  return 0;
}
