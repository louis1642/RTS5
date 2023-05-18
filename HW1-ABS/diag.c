//------------------- DIAG.C ---------------------- 

#define	_GNU_SOURCE	//per settare affinity (su quale CPU eseguono i thread)
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <string.h>
#include <sched.h>	// affinity
#include "parameters.h"

int main(void) {

    struct sched_param sp;
    sp.sched_priority = 20;
    sched_setscheduler(0, SCHED_FIFO, &sp);

    // AFFINITY
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    // CODE
    mqd_t qd_diag_request;
    mqd_t qd_diag_response;

    char msg_req[] = "Requesting info";

    // apro la coda di req in sola scrittura
    // la coda dovrebbe essere stata già aperta dal controller
    struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_DIAG_MSG_SIZE; 
	attr.mq_curmsgs = 0;
    if ((qd_diag_request = mq_open(DIAG_REQUEST_QUEUE_NAME, O_WRONLY, QUEUE_PERMISSIONS, &attr)) == -1) {
        perror("diag: cannot open queue qd_diag_request");
        exit(1);
    }
    // apro la coda di res in sola lettura
    if ((qd_diag_response = mq_open(DIAG_RESPONSE_QUEUE_NAME, O_RDONLY, QUEUE_PERMISSIONS, &attr)) == -1) {
        perror("diag: cannot open queue qd_diag_response");
        exit(1);
    }

    char msg_res[MAX_DIAG_MSG_SIZE];
    if (mq_send(qd_diag_request, msg_req, strlen(msg_req) + 1, 0) == -1) {
        perror("diag: cannot send request message");
    } else {
        // messaggio inviato
        printf("Message sent!\n");
        // aspetto una risposta
        if (mq_receive(qd_diag_response, msg_res, MAX_DIAG_MSG_SIZE, 0) == -1) {
            perror("diag: cannot receive message");
        } else {
            // messaggio ricevuto
            printf("Message received!\n");
            // stampo il messaggio
            printf("%s\n", msg_res);
        }
    }


    // chiusura delle code
    if (mq_close(qd_diag_request) == -1) {
        perror("diag: cannot close queue qd_diag_request");
        exit(1);
    }
    if (mq_close(qd_diag_response) == -1) {
        perror("diag: cannot close queue qd_diag_response");
        exit(1);
    }

	return 0;
}

