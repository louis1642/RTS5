//------------------- PLANTR.C ---------------------- 

/* affinity non settata: la macchina è un oggetto fisico,
 * non una simulazione, pertanto non ha senso che sia "prevedibile"
 * come i thread del controllore.
 */

#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include "rt-lib.h"
#include "parameters.h"

static int keep_on_running = 1;

struct shared_int {
	int value;
	pthread_mutex_t lock;
};
static struct shared_int shared_sensor;
static struct shared_int shared_actuator;

/* emula l'impianto da controllare */
void * plant_loop(void * par) {
	
	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th,TICK_TIME);

  	srand(time(NULL));

	shared_sensor.value = 100; 
	shared_actuator.value = 0;

	// velocità del veicolo
	int sensor = shared_sensor.value;

	int count = 1;
	int noise = 0;
	int skate = 0;
	int brake = 0;
	int when_to_decrease = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
	while (keep_on_running) {
		wait_next_activation(th);

		pthread_mutex_lock(&shared_sensor.lock);
		sensor = shared_sensor.value;
		pthread_mutex_unlock(&shared_sensor.lock);

		// se non in sbandata, ogni 1-10 step decrementa la velocità
		if (count%when_to_decrease == 0 && !skate) {
			if (sensor > 0) {
				sensor--; // il dato scende in maniera non lineare...
			}
			when_to_decrease = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
			count = when_to_decrease;
		}

		// aggiunge rumore tra -1 e 1	
		if (count%3 == 0 && !skate) {
			noise = -1 + (int)(3.0 * rand() / ( RAND_MAX + 1.0 ));
			if (sensor > 0) {
				sensor += noise;
			}
		}
		
		// reazione al controllo
		if (count%2 == 0) {
			pthread_mutex_lock(&shared_actuator.lock);
			if (shared_actuator.value == 0) {							// continua
				brake = 0; 
				skate = 0;
			} else if (shared_actuator.value == 1) {					// accelera
				sensor++; 
				brake = 0; 
				skate = 0;
			} else if ((shared_actuator.value == -1) && (sensor>0)) {	// decelera
				sensor--; 
				brake = 0; 
				skate = 0;
			} else if ((shared_actuator.value== -2) && (sensor>0)) { 	// frena
				// emula frenata e skating
				brake++;
				if (brake < 10) { // all'inizio la frenata funziona
					if (sensor > 2) {
						// per i primi 10 step, decrementa la velocità di 3 (frenata veloce)
						sensor -= 3;
					} else {
						sensor = 0;
					}
				} else { //dopo, inizia lo skating, con probabilità 9/10
					skate = (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ))>=2;
					if (!skate) { 	// se non sono in skate, freno bene
						if (sensor > 2) {
							sensor -= 3; 
						} else {
							sensor = 0;
						}
					}
				}
			}
			pthread_mutex_unlock(&shared_actuator.lock);
		}
		pthread_mutex_lock(&shared_sensor.lock);
		shared_sensor.value = sensor;
		pthread_mutex_unlock(&shared_sensor.lock);
		count++;
	}

	return 0;
}

// legge il valore del sensore di velocità dalla memoria condivisa e lo scrive sulla coda
void * sensor_driver_loop(void* par) {
	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th, TICK_TIME);

	// Messaggio da inviare al controller
	char message[MAX_MSG_SIZE];
		
	/* Coda */
	struct mq_attr attr;

	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo una coda in sola scrittura (O_WRONLY), se non esiste la creiamo (O_CREAT)
	// La coda conterrà lo stato del plant
	mqd_t sensor_qd;
	if ((sensor_qd = mq_open(SENSOR_R_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("Sensor driver loop: mq_open (sensor)");
		exit(1);
	}
	int value = 0;
	while (keep_on_running) {
		wait_next_activation(th);
		pthread_mutex_lock(&shared_sensor.lock);
		value = shared_sensor.value;
		pthread_mutex_unlock(&shared_sensor.lock);

		printf("Sensor R: %d\n",value);	//DEBUG

		sprintf(message, "%d", value);
		if (mq_send(sensor_qd, message, strlen(message) + 1, 0) == -1) {
		    perror("Sensor driver: Not able to send message to controller");
		    continue;
		}
	}

	/* Clear */
    if (mq_close(sensor_qd) == -1) {
        perror("Sensor driver: mq_close sensor_qd");
        exit(1);
    }

	return 0;
}


// riceve il comando di controllo sulla coda e lo scrive nella struttura dati shared_actuator
void * actuator_driver_loop(void* par) {
	// Messaggio ricevuto dal controller
	char message[MAX_MSG_SIZE];
		
	/* Coda */
	struct mq_attr attr;

	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo una coda in sola lettura (O_RDONLY), se non esiste la creiamo (O_CREAT)
	// La coda conterrà il segnale di controllo
	mqd_t actuator_qd;
	if ((actuator_qd = mq_open(ACTUATOR_R_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("Actuator driver loop: mq_open (actuator)");
		exit(1);
	}
	int control = 0;
	while (keep_on_running) {
		if (mq_receive(actuator_qd, message, MAX_MSG_SIZE, NULL) == -1){
			perror("Actuator driver loop: mq_receive (actuator)");	
			break;						//DEBUG
		} else { 
			control = atoi(message);
			pthread_mutex_lock(&shared_actuator.lock);
			shared_actuator.value = control;
			pthread_mutex_unlock(&shared_actuator.lock);
			//printf("\t\t\t\tAcutator: %d\n",control);//DEBUG
		}
	}

	/* Clear */
    if (mq_close(actuator_qd) == -1) {
        perror("Actuator driver: mq_close actuator_qd");
        exit(1);
    }

	return 0;
}

int main(void) {
	printf("The plant R STARTED!\n");

	pthread_t plant_thread;
	pthread_t sensor_driver_thread;
	pthread_t actuator_driver_thread;

	pthread_mutex_init(&shared_sensor.lock, NULL);
	pthread_mutex_init(&shared_actuator.lock, NULL);

	pthread_attr_t myattr;
	struct sched_param myparam;

	pthread_attr_init(&myattr);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED); 

	// PLANT THREAD
	periodic_thread plant_th;
	plant_th.period = TICK_TIME;
	plant_th.priority = 50;

	myparam.sched_priority = plant_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&plant_thread, &myattr, plant_loop, ( void*)&plant_th);

	// SENSOR DRIVER THREAD
	periodic_thread sensor_th;
	sensor_th.period = TICK_TIME;
	sensor_th.priority = 50;

	myparam.sched_priority = sensor_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&sensor_driver_thread, &myattr, sensor_driver_loop, (void*)&sensor_th);

	// ACTUATOR DRIVER THREAD
	myparam.sched_priority = 51;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&actuator_driver_thread, &myattr, actuator_driver_loop, NULL);

	pthread_attr_destroy(&myattr);
	
	
	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') break;
  	}

	// segnala a tutti i thread di interrompere la loro attività
	keep_on_running = 0;

	if (mq_unlink (SENSOR_R_QUEUE_NAME) == -1) {
        perror("Main: mq_unlink sensor queue");
        exit(1);
    }

	if (mq_unlink (ACTUATOR_R_QUEUE_NAME) == -1) {
        perror("Main: mq_unlink actuator queue");
        exit(1);
    }

 	printf("The plant R STOPPED\n");
	return 0;
}




