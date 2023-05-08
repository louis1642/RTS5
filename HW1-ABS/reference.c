#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include "parameters.h"

int main(int argc, char ** argv)
{
    if (argc != 2) {	
	    printf("Usage: reference <val>\n");
	    return -1;
    }

    //Apre coda per la reference
    mqd_t reference_qd;
    if ((reference_qd=mq_open(REFERENCE_QUEUE_NAME,O_WRONLY))==-1) {
        perror("reference mqopen reference_qd");
        return -1;
    }

    if(mq_send(reference_qd,argv[1],strlen(argv[1])+1,0)==-1) {
        perror("reference send reference_qd");
        return -1;
    }

    printf("Reference set to: %s\n",argv[1]);

    if (mq_close (reference_qd) == -1) {
        perror ("reference: mq_close reference_qd");
        return -1;
    }
    return 0;
}