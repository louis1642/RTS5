#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mqueue.h>
#include <string.h>
#include <sched.h>

#define SLEEP_TIME_S 1 
#define PS_QUEUE_NAME   "/ps_queue"


int main(void){
	int start = 1;
	
	// Setto la priorità e lo scheduler del processo
	struct sched_param sp;
	sp.sched_priority = 20;
	sched_setscheduler(0, SCHED_FIFO, &sp);
	
	// descrittore della coda
	mqd_t qd_ps;
	
	char message[64];
	sprintf (message, "message from: %d", getpid ());
	
	// Apriamo una coda in sola scrittura (O_WRONLY), la coda dovrebbe essere già stata creata dal server. La coda serve ad inviare messaggi al server.
	if ((qd_ps = mq_open (PS_QUEUE_NAME, O_WRONLY)) == -1) {
		perror ("Client: mq_open (server)");
		exit (1);
	}
	
	do{
		printf("which will it be?\n");
		printf("0.exit\n");
		printf("1.start aperiodic request\n");
		printf(">");
		scanf("%d",&start);
		// There should be a control for not numeric input
		if(start == 1){
			printf("Sending request...\n");
			if (mq_send (qd_ps, message, strlen(message) + 1, 0) == -1) {
        			perror ("async_request: Not able to send message to ps");
			}
		}
		else{
			start = 0; 		
		}

	}while(start);
	
	/* Clear */
	if (mq_close (qd_ps) == -1) {
		perror ("rt_async: mq_close qd_ps");
		exit (1);
	}	

	return 0;
}
