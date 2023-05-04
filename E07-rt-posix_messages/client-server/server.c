/*
 * Server 
 * un esempio di interprocess commnuication 
 * tramite messaggi POSIX
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

// il nome delle code deve iniziare con / e finire con il carattere di terminazione (convenzione POSIX)
#define SERVER_QUEUE_NAME   "/sp-example-one-server"
// i permessi sono definiti in ottale (come permessi linux), red-write-execute per owner-group-world
// inizia per 0 perche' e' la notazione ottale (0b100 binaria 0x100 esadecimale)
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 100
#define MAX_MSG_SIZE 256

int main (int argc, char **argv) {
    mqd_t qd_server, qd_client;  	// descrittori delle code
    long token_number = 1; 	   	// Token passato ad ogni richiesta del CLient

    printf ("Server alive!\n");

    // Vanno definiti gli attributi che andremo ad assegnare alla coda
    struct mq_attr attr;

    attr.mq_flags = 0;				
    attr.mq_maxmsg = MAX_MESSAGES;	
    attr.mq_msgsize = MAX_MSG_SIZE; 
    attr.mq_curmsgs = 0;

    // Apriamo una coda in sola lettura (O_RDONLY) e se non esiste la creiamo (O_CREAT).
    // Il nome dato alla coda è una stringa: "/sp-example-server". Può essere una qualsiasi stringa l'importante è che essa inizi per "/"
    if ((qd_server = mq_open (SERVER_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
        perror ("Server: mq_open (server)");
        exit (1);
    }
    char in_buffer [MAX_MSG_SIZE];
    char out_buffer [MAX_MSG_SIZE];
    char end []="q";

    while (1) {
        // Ricevo il messaggio più vecchio in coda con priorità più alta
        // mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio);
        // in questo caso NULL perchè non stiamo usando le priorità
        if (mq_receive (qd_server, in_buffer, MAX_MSG_SIZE, NULL) == -1) {
            perror ("Server: mq_receive");
            exit (1);
        }

        printf ("Server: message received\n");

	    // Verifico la richiesta di chiusura del server
        if (strncmp(in_buffer,"q", sizeof(in_buffer)) == 0) {
            break;
        } else {
            // Rispondo al Client usando una diversa coda.
            // Il nome della coda del client la ricavo dal messaggio inviato dal client stesso
            // Non abbiamo messo O_CREATE: se la coda non esiste, diamo errore
            if ((qd_client = mq_open (in_buffer, O_WRONLY)) == -1) {
                perror ("Server: Not able to open client queue");
                continue;
            }

            // dobbiamo convertire il long int token_number in una stringa out_buffer
            // (le code usano le stringhe)
            sprintf (out_buffer, "%ld", token_number);
            // Invio il token al client	
            // la lunghezza del messaggio è pari alla lunghezza della stringa + 1 (il carattere di terminazione)
            if (mq_send (qd_client, out_buffer, strlen(out_buffer) + 1, 0) == -1) {
                perror ("Server: Not able to send message to client");
                continue;
            }
            if (mq_close (qd_client) == -1) {
                perror ("Server: mq_close client");
                exit (1);
            }
                    
            printf ("Server: response sent to client.\n");
            token_number++;
        }
    }
    
    // l'uscita dal ciclo è a sequito dell'immissione di 'q'

    /* Clear */
    if (mq_close (qd_server) == -1) {
        perror ("Server: mq_close qd_server");
        exit (1);
    }

    // ricorda: unlink significa "se nessuno la sta più usando, puoi chiudere la coda"
    if (mq_unlink (SERVER_QUEUE_NAME) == -1) {
        perror ("Server: mq_unlink server queue");
        exit (1);
    }
   
    printf("Server: bye!\n");
}
