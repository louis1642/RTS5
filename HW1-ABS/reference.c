//------------------- REFERENCE.C ---------------------- 

#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include "parameters.h"

int main(int argc, char ** argv) {
    if (argc != 2) {	
	    printf("Usage: reference <val>\n");
	    return -1;
    }

    // Apre code per le reference_r e reference_l 
    mqd_t reference_r_qd;
    if ((reference_r_qd = mq_open(REFERENCE_R_QUEUE_NAME, O_WRONLY)) == -1) {
        perror("reference mqopen reference_r_qd");
        return -1;
    }
    if (mq_send(reference_r_qd, argv[1], strlen(argv[1])+1, 0) == -1) {
        perror("reference send reference_r_qd");
        return -1;
    }

    mqd_t reference_l_qd;
    if ((reference_l_qd = mq_open(REFERENCE_L_QUEUE_NAME, O_WRONLY)) == -1) {
        perror("reference mqopen reference_l_qd");
        return -1;
    }
    if (mq_send(reference_l_qd, argv[1], strlen(argv[1])+1, 0) == -1) {
        perror("reference send reference_l_qd");
        return -1;
    }

    printf("Reference set to: %s\n", argv[1]);

    // chiusura code (unlink fatta nel controller)
    if (mq_close(reference_r_qd) == -1) {
        perror("reference: mq_close reference_r_qd");
        return -1;
    }
    if (mq_close(reference_l_qd) == -1) {
        perror("reference: mq_close reference_l_qd");
        return -1;
    }

    return 0;
}