/*
 * Client 

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
#include <errno.h>   // for errno
#include <limits.h>  // for INT_MAX, INT_MIN
#include <stdlib.h>  // for strtol

#define SERVER_QUEUE_NAME   "/sp-example-two-server"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256

int main (int argc, char **argv)
{
	char client_queue_name [64];
	mqd_t qd_server, qd_client;   // Descrittore delle code
	char in_buffer [MAX_MSG_SIZE];
	int priority = 0;
	char *p;
	errno = 0;

	// Leggi input per la priority
	if (argc > 1) {
		long conv = strtol(argv[1], &p, 10);

		// Verifico che la stringa in ingresso sia un numero e non un carattere e inoltre verifico che sia nel giusto intervallo (0-99)
		if (errno != 0 || *p != '\0' || conv > 99 || conv < 0) {
			//error
			printf("Error in input, priority: %d\n", priority);
		} else {
			// No error
			priority = conv;
			printf("Client with message priority: %d\n", priority);
		}
		
	}


	sprintf (client_queue_name, "/sp-example-two-client-%d", getpid ());

	// Vanno definiti gli attributi che andremo ad assegnare alla coda
	struct mq_attr attr;

	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_MESSAGES;
	attr.mq_msgsize = MAX_MSG_SIZE;
	attr.mq_curmsgs = 0;

	// Apriamo una coda in sola lettura (O_RDONLY) e se non esiste la creiamo (O_CREAT). La coda serve a ricevere le risposte del server
	if ((qd_client = mq_open (client_queue_name, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("Client: mq_open (client)");
		exit (1);
	}

	// Apriamo una coda in sola scrittura (O_WRONLY), la coda dovrebbe essere già stata creata dal server. La coda serve ad inviare messaggi al server.
	if ((qd_server = mq_open (SERVER_QUEUE_NAME, O_WRONLY)) == -1) {
		perror ("Client: mq_open (server)");
		exit (1);
	}

	printf ("Ask for a token (Press <ENTER>): ");

	//Aspetto di ricevere un input da tastiera
	while (getchar()!='q') {

		// Invio un messaggio al server contente il nome della coda sulla quale rispondere
		if (mq_send (qd_server, client_queue_name, strlen (client_queue_name) + 1, priority) == -1) {
			perror ("Client: Not able to send message to server");
			continue;
		}

		// Ricevo la risposta del server
		if (mq_receive (qd_client, in_buffer, MAX_MSG_SIZE, NULL) == -1) {
			perror ("Client: mq_receive");
			exit (1);
		}
		// Mostro il token ricevuto dal server
		printf ("Client: Token received from server: %s\n\n", in_buffer);

		printf ("Ask for a token (Press <ENTER>) [q to exit]: ");
	}

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
		perror ("Client: mq_close");
		exit (1);
	}

	if (mq_unlink (client_queue_name) == -1) {
		perror ("Client: mq_unlink");
		exit (1);
	}
	printf ("Client: bye\n");

	exit (0);
}
