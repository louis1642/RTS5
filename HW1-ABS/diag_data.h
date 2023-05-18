//------------------- DIAG_DATA.H ---------------------- 

#ifndef DIAG_DATA
#define DIAG_DATA

#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parameters.h"

// dati di diagnostica 
struct diag_data {
    char name[8];
    unsigned long int WCET;
    int avg_sensor;
    int control;
    int control_action;
    int reference;
};

// dati di diagnostica protetti da mutex 
struct shared_diag_data {
    struct diag_data data;
    pthread_mutex_t lock;
};

// converte i dati in un array di caratteri
void dtc(struct diag_data data, char string[]); 

// concatena i dati ad una stringa di caratteri 
void append_to_string(struct diag_data* data, char string[]);

// inizializza i dati di diagnostica del thread "name" 
void init_diag_data(struct diag_data* data, char name[]);


#endif