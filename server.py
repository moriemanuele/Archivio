#! /usr/bin/env python3
import logging, socket, argparse, sys, threading, logging, os, struct, concurrent.futures, signal, subprocess
Description = """Server in python di Emanuele Mori"""
HOST = "127.0.0.1"  
PORT = 58005   
Max_sequence_length = 2048

#configuro il logging
# configurazione del logging
# il logger scrive su un file con nome uguale al nome del file eseguibile
logging.basicConfig(filename=os.path.basename('server.log'),
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')



def main(numt, numr, numw):
  #creo le pipe con nome
  if (not os.path.exists("capolet")) and (not os.path.exists("caposc")):
    try:
      os.mkfifo("capolet")
      os.mkfifo("caposc")
    except OSError as e:
      logging.exception("Errore durante la creazione di una delle pipe")
      sys.exit(1)
  else:
    logging.exception("Almeno una delle due pipe esiste già")
    sys.exit(1)
  

  if args.usovalgrind:
    p = subprocess.Popen(["valgrind","--leak-check=full", 
                      "--show-leak-kinds=all", 
                      "--log-file=valgrind-%p.log", 
                      "./archivio", str(numw),str(numr)])
  else:
    p = subprocess.Popen(["./archivio", str(numw),str(numr)])
  
  
  #apro le pipe per la scrittura
  capolet = os.open("capolet", os.O_WRONLY)
  caposc = os.open("caposc", os.O_WRONLY) 

  #creo i mutex
  lockcapolet = threading.Lock()
  lockcaposc = threading.Lock()
  locklog = threading.Lock()

  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
    try:
      # permette di riutilizzare la porta se il server viene chiuso
      server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)      
      server.bind((HOST, PORT))
      server.listen()
      with concurrent.futures.ThreadPoolExecutor(max_workers = numt) as executor:
        while True:
          # mi metto in attesa di una connessione
          conn, addr = server.accept()
          # l'esecuzione di submit non è bloccante
          # fino a quando ci sono thread liberi
          executor.submit(gestisci_connessione, conn,addr,lockcapolet,lockcaposc,locklog,capolet,caposc)
    except (socket.error, socket.timeout, socket.herror) :
      #gestisco il caso in cui server.bind((HOST, PORT)) dia errore
      logging.exception("Errore con la connessione via socket")
      #faccio unlink delle pipe 
      os.unlink("caposc")
      os.unlink("capolet")
      #chiudo le pipe
      os.close(caposc)
      os.close(capolet)
      #invio il segnale SIGTERM al programma archivio
      p.send_signal(signal.SIGTERM)
      sys.exit(1)

    except KeyboardInterrupt:
      #gestisci questo segnale
      
      #faccio unlink delle pipe 
      os.unlink("caposc")
      os.unlink("capolet")
      #attendo la terminazione di tutti i thread
      executor.shutdown(wait=True)

      #chiudo le pipe
      os.close(caposc)
      os.close(capolet)

      #invio il segnale SIGTERM al programma archivio
      p.send_signal(signal.SIGTERM)

      #chiudo il socket con shutdown
      server.shutdown(socket.SHUT_RDWR)
  return

def gestisci_connessione(conn,addr,lockcapolet,lockcaposc,locklog,capolet,caposc):
  with conn:
    data = recv_all(conn,4) #mi metto in attesa di 4 byte per vedere se il client è 1 o 2
    tipoconnessione = struct.unpack("!i",data)[0]
    if tipoconnessione==0:
      #gestisco una connessione di tipo A, quindi riceverò una volta sola dimensione e sequenza. il programma client1 farà poi un'altra connessione per la sequenza successiva
      recv_allc1(conn,lockcapolet,locklog,capolet)
    elif tipoconnessione==1:
      #gestisco una connessione di tipo B
      recv_allc2(conn,lockcaposc,locklog,caposc)
    else:
      #c'è qualcosa che non va
      raise socket.error 


def recv_allc1(conn,lockcapolet,locklog,capolet):
  dimensione_sequenza_da_decodificare = recv_all(conn,4)
  dimensionesequenza = struct.unpack("!i",dimensione_sequenza_da_decodificare)[0]
  chunk =  recv_all(conn,dimensionesequenza)

  #vado a scrivere in capolet prima dimensione_sequenza_da_decodificare, che occupa 4 byte, poi chunk che occupa dimensionesequenza byte. per questo serve un mutex mutexcapolet. poi acquisisco il mutex mutexlog per scrivere su server.log f"connessione A, {dimensionesequenza}"
  #scrivo sulla pipe
  lockcapolet.acquire()
  os.write(capolet,dimensione_sequenza_da_decodificare)
  os.write(capolet,chunk)
  lockcapolet.release()

  #ho scritto sulla pipe, ora scrivo su server.log
  locklog.acquire()
  logging.debug(f"A {dimensionesequenza}")
  locklog.release()
  return


def recv_allc2(conn,lockcaposc,locklog,caposc):
  numerosequenzericevute = 0
  while True:
    dimensione_sequenza_da_decodificare = recv_all(conn,4)
    dimensionesequenza = struct.unpack("!i",dimensione_sequenza_da_decodificare)[0]
    chunk =  recv_all(conn,dimensionesequenza)
    if not chunk:
      break
    numerosequenzericevute+=1
    
    #scrivo nella pipe
    lockcaposc.acquire()
    os.write(caposc,dimensione_sequenza_da_decodificare)
    os.write(caposc,chunk)
    lockcaposc.release()
  
    #ho scritto sulla pipe, ora scrivo su server.log
    locklog.acquire()
    logging.debug(f"B {dimensionesequenza}")
    locklog.release()
  conn.sendall(struct.pack("!i",numerosequenzericevute))
  return 
  
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 2048))
    if len(chunk) == 0:
      return chunks
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('numt', help='massimo numero di thread che il server deve utilizzare contemporanemente per la gestione dei client ', type = int)
  parser.add_argument('-r', help='numero di thread lettori', type = int, default=3)  
  parser.add_argument('-w', help='numero di thread scrittori', type = int, default=3) 
  parser.add_argument("-v",'--usovalgrind',help ='chiama il programma archivio mediante valgrind',action = "store_true")
  args = parser.parse_args()
  assert args.numt > 0, "Il numero di thread deve essere positivo"
main(args.numt,args.r,args.w)
