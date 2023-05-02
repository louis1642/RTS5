#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#define MONITOR_QUEUE_NAME "/monitor_queue"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256



int main (int argc, char **argv)
{
	printf ("Monitor alive!\n");
	
	mqd_t qd_monitor;
	
	// Messaggio da ricevere dal polling server
	char in_buffer [MAX_MSG_SIZE];
	
	// Vanno definiti gli attributi che andremo ad assegnare alla coda
	struct mq_attr attr;

	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;

	// Apriamo una coda in sola lettura (O_RDONLY), se non esiste la creiamo (O_CREAT).
	if ((qd_monitor = mq_open (MONITOR_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("Monitor: mq_open (Monitor)");
		exit (1);
	}
	

	while (1) {
		// Attendo di ricevere un messaggio. Se ci sono più messaggi verrà ritirato il più vecchio in coda con priorità più alta.  
		if(mq_receive(qd_monitor, in_buffer, MAX_MSG_SIZE, NULL) == -1){
			perror ("Monitor: Not able to open client queue");
			break;
		}
		else{
			// Verifico la richiesta di chiusura del server
			if(strncmp(in_buffer,"q", sizeof(in_buffer)) == 0){
				break;
			}
			else{
				printf ("Aperiodic Message received!\n");
			}
		}
		
	}
	
	/* Clear */
	if (mq_close (qd_monitor) == -1) {
		perror ("Monitor: mq_close qd_monitor");
		exit (1);
	}

	if (mq_unlink (MONITOR_QUEUE_NAME) == -1) {
		perror ("Monitor: mq_unlink monitor queue");
		exit (1);
	}	
    
	printf("EXIT!\n");
	
	return 0;
}
