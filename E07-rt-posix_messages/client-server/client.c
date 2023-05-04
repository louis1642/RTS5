/*
 * Client 
 * un esempio di interprocess commnuication 
 * tramite messaggi POSIX
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#define SERVER_QUEUE_NAME   "/sp-example-one-server"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256

int main (int argc, char **argv) {
    char client_queue_name [64];
    mqd_t qd_server, qd_client;   // Descrittore delle code


    sprintf(client_queue_name, "/sp-example-one-client-%d", getpid());

    // Vanno definiti gli attributi che andremo ad assegnare alla coda
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // Apriamo una coda in sola lettura (O_RDONLY) e se non esiste la creiamo (O_CREAT).
    // La coda serve a ricevere le risposte del server
    if ((qd_client = mq_open (client_queue_name, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
        perror ("Client: mq_open (client)");
        exit (1);
    }
   
    // Apriamo una coda in sola scrittura (O_WRONLY), la coda dovrebbe essere già stata creata dal server.
    // La coda serve ad inviare messaggi al server.
    if ((qd_server = mq_open (SERVER_QUEUE_NAME, O_WRONLY)) == -1) {
        perror ("Client: mq_open (server)");
        exit (1);
    }

    char in_buffer [MAX_MSG_SIZE];

    printf ("Ask for a token (Press <ENTER>) [q to exit]: ");

    //Aspetto di ricevere un input da tastiera
    while (getchar()!='q') {
 
        // Invio un messaggio al server contente il nome della coda sulla quale rispondere
        if (mq_send (qd_server, client_queue_name, strlen (client_queue_name) + 1, 0) == -1) {
            perror ("Client: Not able to send message to server");
            continue;
        }

        // Ricevo la risposta del server
        // E' una recieve BLOCCANTE
        if (mq_receive (qd_client, in_buffer, MAX_MSG_SIZE, NULL) == -1) {
            perror ("Client: mq_receive");
            exit (1);
        }
        // Mostro il token ricevuto dal server
        printf ("Client: Token received from server: %s\n\n", in_buffer);
        
        printf ("Ask for a token (Press <ENTER>) [q to exit]: ");
    }
    
    // uscita dal while = ho ricevuto 'q'

    // Invio un messaggio di terminazione al server
    if (mq_send (qd_server, "q", sizeof(char) + 1, 0) == -1) {
	    perror ("Client: Not able to send message to server");
    }

    /* Clear */
    if (mq_close (qd_server) == -1) {
        perror ("Client: mq_close qd_server");
        exit (1);
    }

    if (mq_close (qd_client) == -1) {
        perror ("Client: mq_close qd_client");
        exit (1);
    }

    if (mq_unlink (client_queue_name) == -1) {
        perror ("Client: mq_unlink client queue");
        exit (1);
    }
    printf ("Client: bye\n");

    exit (0);
}
