## Archivio di Emanuele Mori, 638005

# archivio.c
Il codice inizia con la definizione della struct `coppia`, necessaria per la deallocazione del contenuto della tabella hash. Viene poi dichiarata una funzione per rendere thread-async-safe la stampa del numero di entry durante la gestione dei segnali.
Come indicato nelle istruzioni, i thread capolettori e caposcrittori leggono tramite pipe prima la dimensione della sequenza che stanno per ricevere poi la sequenza stessa, ne fanno la tokenizzazione e mettono una copia di ogni stringa sul buffer produttori-consumatori, che ho implementato usando variabili di condizione in base al principio di modularità. I lettori e gli scrittori eseguono le operazioni di `aggiungi()` e `conta()` in mutua esclusione, deallocando in seguito le stringhe ricevute dal buffer produttori-consumatori. Sempre per il principio di modularità ho deciso di rendere la testa della lista di `ENTRY` ed il rispettivo contatore due variabili locali, in mano solamente al thread gestore e agli scrittori. Avrei potuto dichiararle fuori dal main con la parola chiave `static` e renderle visibile solo in `archivio` ma in questo modo ogni altro thread avrebbe potuto accedervi. Di conseguenza il prototipo di `aggiungi()` è diverso: oltre ad una stringa prende in input sia il puntatore al contatore sia il puntatore alla testa della lista.
Poiché il meccanismo di scrittura e lettura nel buffer produttori-consumatori è identico sia per gli scrittori sia per i lettori ed i rispettivi capi, ho implementato quattro funzioni: ognuna per rilasciare o acquisire l'accesso in lettura o scrittura. In questo modo ho reso più comprensibile il codice ed evitato di ripetere inutilmente righe di codice, sfruttando i principi di modularità ed astrazione, fondamentali per lo sviluppo di programmi complessi. D'altra parte, per la gestione della tabella hash ho preferito duplicare il codice in modo da rendere più veloce la gestione dei segnali.
Per assicurarmi che il programma termini solamente quando tutti i thread hanno terminato, il `main` prima di eseguire `return 0` attende la terminazione del thread gestore. Quest'ultimo una volta ricevuto SIGTERM attende la terminazione di capolettore e caposcrittore, i quali rispettivamente prima di terminare aspettano la terminazione dei thread lettori e scrittori. 
Nel caso della gestione dei segnali `SIGINT` e `SIGUSR1` ho preferito curarmi della thread-safety piuttosto che della thead-async-safety, visto che senza la mutua esclusione avrei molto probabilmente prodotto un valore sbagliato; mentre se durante la gestione di un segnale viene ricevuto un altro segnale, quest'ultimo sarà gestito solamente una volta terminata la gestione del primo. Infine, per quanto riguarda `SIGTERM` e `SIGUSR1` ho deallocato il contenuto della tabella hash gestendo una lista. 

# server.py
Il `main` viene lanciato usando il modulo `argparse`. Dopo aver creato le pipe, viene lanciato `archivio` con o senza valgrind in base a `-v`. In seguito all'inizializzazione delle pipe e lock, si hanno due blocchi `with`: il primo per la gestione socket ed il secondo per il modulo `ThreadPoolExecutor`, nel quale viene lanciato un thread dedicato per ogni connessione per un massimo di `numt` thread. Il thread dedicato per prima cosa si mette in attesa di un intero: se è 0 allora la connessione sarà di tipo A e dunque sarà chiamata `recv_allc1`, altrimenti la connessione sarà di tipo B e di conseguenza sarà chiamata `recv_allc2`. Entrambe queste funzioni ricorrono a `recv_allc`, per avere la sicurezza che tutti i dati vengano ricevuti correttamente. In aggiunta a ciò, entrambe le funzioni prima ricevono la dimensione della sequenza e poi la sequenza stessa, ed in mutua esclusione scrivono su pipe i valori ricevuti. Né la dimensione né le stringhe vengono spedite decodificate, dal momento che sarà archivio a farne la decodifica. 
Il primo blocco `with` non gestisce soltanto l'interruzione di una `KeyboardInterrupt`, ma anche tutti gli errori dovuti alla gestione socket: viene fatto l'unlink delle pipe, vengono chiuse e viene spedito SIGTERM per notificare ad archivio di terminare. 
Infine, ho deciso che nonostante la scrittura sulle pipe e sul file `server.log` siano comandi svolti uno dopo l'altro, fosse buona pratica eseguire le due istruzioni con due lock differenti per rendere il programma il meno sequenziale possibile.
Il modulo di logging è stato usato anche per gestire eventuali eccezioni.

# client1 e client2
I due programmi hanno un codice molto simile: entrambi prima spediscono il tipo di connessione, leggono da file usando la funzione `getline`, spediscono la dimensione della sequenza letta ed in seguito la sequenza stessa. Entrambi ricorrono a `writen` e `readn`, che per evitare di replicare codice sono dichiarate all'interno di `xerrori.c`.  `client1` una volta uscito dal while si mette in attesa di un intero, lo decodifica e verifica che sia uguale al numero di sequenze spedite.

# altro
Le costanti usate in più programmi sono state dichiarate in `xerrori.h` in modo da non duplicare codice. Ho deciso di sfruttare le funzioni del file `xerrori.c` affinché l'utente possa avere una migliore comprensione di dove si è verificato un eventuale errore.