/*
 * Server 
 * un esempio di interprocess commnuication 
 * tramite messaggi POSIX
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#define SERVER_QUEUE_NAME   "/sp-example-two-server"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256


int main (int argc, char **argv)
{
	mqd_t qd_server, qd_client;  	// descrittori delle code
	long token_number = 1; 	   	// Token passato ad ogni richiesta del CLient
	char temp_buf [10];
	char in_buffer [MAX_MSG_SIZE];
	char out_buffer [MAX_MSG_SIZE];
	
	printf ("Server alive!\n");

	// Vanno definiti gli attributi che andremo ad assegnare alla coda
	struct mq_attr attr;

	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;

	// Apriamo una coda in sola lettura (O_RDONLY) e se non esiste la creiamo (O_CREAT). Inoltre la coda sarà non bloccante (O_NONBLOCK)
	// Il nome dato alla coda è una stringa: "/sp-example-server". Può essere una qualsiasi stringa l'importante è che essa inizi per "/"
	if ((qd_server = mq_open (SERVER_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("Server: mq_open (server)");
		exit (1);
	}
	

	while (1) {
	
		// Ricevo, se esiste, il messaggio più vecchio in coda con priorità più alta. Altrimenti attendo un input da tastiera. 
		if (mq_receive(qd_server, in_buffer,MAX_MSG_SIZE,NULL) == -1){
			printf ("Press <ENTER> to receive buffered messages: ");

	    		//Aspetto di ricevere un input da tastiera per ricevere i messaggi
			while (fgets (temp_buf, 2, stdin)){
				break;
			}
			
		}
		else{
			printf ("Server: message received.\n");

			// Verifico la richiesta di chiusura del server
			if(strncmp(in_buffer,"q", sizeof(in_buffer)) == 0){
				break;
			}
			else{
				// Rispondo al Client usando una diversa coda. Il nome della coda del client la ricavo dal messaggio inviato dal client stesso
				if ((qd_client = mq_open (in_buffer, O_WRONLY)) == 1) {
					perror ("Server: Not able to open client queue");
					continue;
				}

				sprintf (out_buffer, "%ld", token_number);
				// Invio il token al client	
				if (mq_send (qd_client, out_buffer, strlen (out_buffer) + 1, 0) == -1) {
					perror ("Server: Not able to send message to client");
					continue;
				}
				if (mq_close (in_buffer) == -1) {
					perror ("Server: mq_close client");
					exit (1);
				}
				printf ("Server: response sent to client.\n");
				token_number++;
			}
		}
	
	}

	/* Clear */
	if (mq_close (qd_server) == -1) {
		perror ("Server: mq_close qd_server");
		exit (1);
	}

	if (mq_unlink (SERVER_QUEUE_NAME) == -1) {
		perror ("Server: mq_unlink server queue");
		exit (1);
	}

	printf("Server: bye!\n");
}
