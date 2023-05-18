//------------------- CONTROLLER.C ---------------------- 

#define	_GNU_SOURCE	//per settare affinity (su quale CPU eseguono i thread)
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>	// affinity
#include "rt-lib.h"
#include "parameters.h"
#include "diag_data.h"


//emulates the controller

static int keep_on_running = 1;		// usata per fermare i cicli dei task periodici

struct shared_int {
	int value;
	pthread_mutex_t lock;
};
static struct shared_int shared_avg_sensor_r;
static struct shared_int shared_skate_r;
static struct shared_int shared_avg_sensor_l;
static struct shared_int shared_skate_l;
static struct shared_int shared_control_r;
static struct shared_int shared_control_l;

int buffer_r[BUF_SIZE];		// conserva i 3 valori più recenti acquisiti
int buffer_l[BUF_SIZE];
int head_r = 0;	// testa del buffer
int head_l = 0;


// struct diagnostica (definita in diag_data.h)
static struct shared_diag_data sh_sensor_r_diag;
static struct shared_diag_data sh_sensor_l_diag;
static struct shared_diag_data sh_ctrl_r_diag;
static struct shared_diag_data sh_ctrl_l_diag;
static struct shared_diag_data sh_actuator_r_diag;
static struct shared_diag_data sh_actuator_l_diag;

void * acquire_filter_loop_r(void * par) {
	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th,TICK_TIME);

	// Messaggi da prelevare dai driver
	char message_r[MAX_MSG_SIZE];

	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo la coda sensor del plant R in lettura 
	mqd_t sensor_r_qd;
	if ((sensor_r_qd = mq_open(SENSOR_R_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("acquire filter loop: mq_open");
		exit(1);
	}

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_sensor_r_diag.lock);
	init_diag_data(&sh_sensor_r_diag.data, "SNS R");
	pthread_mutex_unlock(&sh_sensor_r_diag.lock);

	// timespec per calcolare il WCET
	struct timespec t_start, t_end;

	// variabili per calcolo media valori
	unsigned int sum_r = 0;
	unsigned int lastAvg = 0;
	int cnt_r = BUF_SIZE;

	
	while (keep_on_running) {
		wait_next_activation(th);
		//calcolo del tempo iniziale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);	

		// PRELIEVO DATI dalla coda del plantR
		if (mq_receive(sensor_r_qd, message_r, MAX_MSG_SIZE, NULL) == -1) {
			perror("acquire filter loop: mq_receive (actuator)");	
			break;				
		} else { 	// dati ricevuti dalla coda 
			buffer_r[head_r] = atoi(message_r);	// atoi() converte una stringa in un integer
			sum_r += buffer_r[head_r];
			head_r = (head_r+1)%BUF_SIZE;	 //coda circolare
			cnt_r--;

			// calcolo media sulle ultime BUF_SIZE letture
			if (cnt_r == 0) {
				cnt_r = BUF_SIZE;
				lastAvg = sum_r/BUF_SIZE;

				//aggiorno la risorsa condivisa 
				pthread_mutex_lock(&shared_avg_sensor_r.lock);
				shared_avg_sensor_r.value = lastAvg;
				pthread_mutex_unlock(&shared_avg_sensor_r.lock);
				sum_r = 0;
			}

			// se tutti i dati nel buffer sono uguali siamo in skate 
			int isSkating = (buffer_r[0] == buffer_r[1] && buffer_r[1] == buffer_r[2]);

			pthread_mutex_lock(&shared_skate_r.lock);
			shared_skate_r.value = isSkating;
			pthread_mutex_unlock(&shared_skate_r.lock);
		}

		//calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);

		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);

		// aggiornamento dati di diagnostica
		pthread_mutex_lock(&sh_sensor_r_diag.lock);
		sh_sensor_r_diag.data.WCET = max(sh_sensor_r_diag.data.WCET, exec_time);
		sh_sensor_r_diag.data.avg_sensor = lastAvg;
		pthread_mutex_unlock(&sh_sensor_r_diag.lock);
	}

	/* Clear */
    if (mq_close(sensor_r_qd) == -1) {
        perror("acquire filter loop: mq_close sensor_r_qd");
        exit(1);
    }

	return 0;
}

void * control_loop_r(void * par) {

	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th, TICK_TIME);
	
	// Messaggio da prelevare dal reference
	char message[MAX_MSG_SIZE];
	
	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo la coda per il reference, in lettura e non bloccante
	mqd_t reference_r_qd;
	if ((reference_r_qd = mq_open(REFERENCE_R_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS,&attr)) == -1) {
		// O_NONBLOCK serve perchè il task di controllo non si blocchi in attesa di un nuovo messaggio
		perror("control loop: mq_open (reference_r)");
		exit(1);
	}

	unsigned int reference_r = 110;

	// conserva il valore filtrato del sensore
	unsigned int plantR_state = 0;
	int error_r = 0;
	unsigned int control_action_r = 0;

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_ctrl_r_diag.lock);
	init_diag_data(&sh_ctrl_r_diag.data, "CTR R");
	pthread_mutex_unlock(&sh_ctrl_r_diag.lock);

	struct timespec t_start,t_end; 

	unsigned int last_state_r = 0;
	int delay_counter_r = 0;

	while (keep_on_running) {
		wait_next_activation(th);
		//calcolo del tempo iniziale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

		// legge il plant state 
		pthread_mutex_lock(&shared_avg_sensor_r.lock);
		plantR_state = shared_avg_sensor_r.value;
		pthread_mutex_unlock(&shared_avg_sensor_r.lock);

		// riceve la reference (Coda non bloccante) 
		if (mq_receive(reference_r_qd, message, MAX_MSG_SIZE, NULL) == -1){
			//printf ("No reference ...\n");	//DEBUG
		} else {
			reference_r = atoi(message);
		}

		// la control action è calcolata localmente affinchè la sezione critica non sia troppo lunga
		// se reference = 0, control action di frenata
		if (reference_r == 0) {
			control_action_r = 4;		// frenare

			// ABS con rilevazione nel sensore
			int skt_r, skt_l;
			pthread_mutex_lock(&shared_skate_r.lock);
			skt_r = shared_skate_r.value;
			pthread_mutex_unlock(&shared_skate_r.lock);
			pthread_mutex_lock(&shared_skate_l.lock);
			skt_l = shared_skate_l.value;
			pthread_mutex_unlock(&shared_skate_l.lock);
			if (skt_r || skt_l) {
				// se almeno una delle due ruote va in skating, allora deceleriamo invece di frenare
				control_action_r = 2;
			}

		} else { // calcolo della legge di controllo
			error_r = reference_r - plantR_state;
			if (error_r > 0) {
				control_action_r = 1;		// accelerare
			} else if (error_r < 0) {
				control_action_r = 2;		// decelerare
			} else {
				control_action_r = 3;		// non fare niente
			}
		}
		
		// aggiorna la control action
		pthread_mutex_lock(&shared_control_r.lock);
		shared_control_r.value = control_action_r;
		pthread_mutex_unlock(&shared_control_r.lock);

		// calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);

		// aggiorno i dati di diagnostica
		pthread_mutex_lock(&sh_ctrl_r_diag.lock);
		sh_ctrl_r_diag.data.WCET = max(sh_ctrl_r_diag.data.WCET, exec_time);
		sh_ctrl_r_diag.data.control_action = control_action_r;
		sh_ctrl_r_diag.data.reference = reference_r;
		pthread_mutex_unlock(&sh_ctrl_r_diag.lock);
	}

	/* Clear */
    if (mq_close(reference_r_qd) == -1) {
        perror("control loop: mq_close reference_r_qd");
        exit(1);
    }
	return 0;
}

void * actuator_loop_r(void * par) {

	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th, TICK_TIME);

	// Messaggio da prelevare dal driver
	char message_r[MAX_MSG_SIZE];

	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo la coda actuator del plant in scrittura
	mqd_t actuator_r_qd;
	if ((actuator_r_qd = mq_open(ACTUATOR_R_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("actuator  loop: mq_open (actuator r)");
		exit(1);
	}

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_actuator_r_diag.lock);
	init_diag_data(&sh_actuator_r_diag.data, "ACT R");
	pthread_mutex_unlock(&sh_actuator_r_diag.lock);
	
	
	struct timespec t_start,t_end; 

	unsigned int control_action_r = 0;	// la control action è salvata in locale
	unsigned int control_r = 0;			// controllo inviato al plant
	while (keep_on_running) {
		wait_next_activation(th);
		// salvo il tempo di thread per valutare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

		// prelievo della control action dalla risorsa condivisa 
		pthread_mutex_lock(&shared_control_r.lock);
		control_action_r = shared_control_r.value;
		pthread_mutex_unlock(&shared_control_r.lock);
		
		switch (control_action_r) {
			case 1:  control_r = 1;  break;		// accelerare
			case 2:	 control_r = -1; break;		// decelerare
			case 3:	 control_r = 0;  break;		// non fare niente
			case 4:  control_r = -2; break;		// frenare
			default: control_r = 0;
		}
		printf("Control R: %d\t\t", control_r); 
		
		// le code comunicano via array di char
		sprintf(message_r, "%d", control_r);	
		//invio del controllo al driver del plant
		if (mq_send(actuator_r_qd, message_r, strlen(message_r) + 1, 0) == -1) {
		    perror("Sensor driver: Not able to send message to controller");
		    continue;
		}

		//calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);
		//aggiorno i dati di diagnostica
		pthread_mutex_lock(&sh_actuator_r_diag.lock);
		sh_actuator_r_diag.data.WCET = max(sh_actuator_r_diag.data.WCET, exec_time);
		sh_actuator_r_diag.data.control = control_r;
		pthread_mutex_unlock(&sh_actuator_r_diag.lock);
		
	}

	/* Clear */
    if (mq_close (actuator_r_qd) == -1) {
        perror("Actuator loop: mq_close actuator_r_qd");
        exit(1);
    }
	return 0;
}

void * acquire_filter_loop_l(void * par) {
	
	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th,TICK_TIME);

	// Messaggi da prelevare dai driver
	char message_l[MAX_MSG_SIZE];

	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo le code sensor dei plant L in lettura 
	mqd_t sensor_l_qd;
	if ((sensor_l_qd = mq_open(SENSOR_L_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("acquire filter loop: mq_open (sensor)");
		exit(1);
	}

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_sensor_l_diag.lock);
	init_diag_data(&sh_sensor_l_diag.data, "SNS L");
	pthread_mutex_unlock(&sh_sensor_l_diag.lock);

	// timespec per calcolare il WCET
	struct timespec t_start, t_end; 

	// variabili per calcolo media valori
	unsigned int sum_l = 0;
	unsigned int lastAvg = 0;
	int cnt_l = BUF_SIZE;


	while (keep_on_running) {
		wait_next_activation(th);
		// calcolo del tempo iniziale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

		// PRELIEVO DATI dalla coda del plantL
		if (mq_receive(sensor_l_qd, message_l, MAX_MSG_SIZE, NULL) == -1) {
			perror("acquire filter loop: mq_receive (actuator)");	
			break;		
		} else {	// dati ricevuti dalla coda 
			buffer_l[head_l] = atoi(message_l);
			sum_l += buffer_l[head_l];
			head_l = (head_l+1)%BUF_SIZE;	// coda circolare
			cnt_l--;

			// calcolo media sulle ultime BUF_SIZE letture
			if (cnt_l == 0) {
				cnt_l = BUF_SIZE;
				lastAvg = sum_l/BUF_SIZE;

				// aggiorno la risorsa condivisa
				pthread_mutex_lock(&shared_avg_sensor_l.lock);
				shared_avg_sensor_l.value = lastAvg;
				pthread_mutex_unlock(&shared_avg_sensor_l.lock);
				sum_l = 0;
			}	

			// se tutti i dati nel buffer sono uguali siamo in skate
			int isSkating = (buffer_l[0] == buffer_l[1] && buffer_l[1] == buffer_l[2]);
			
			pthread_mutex_lock(&shared_skate_l.lock);
			shared_skate_l.value = isSkating;
			pthread_mutex_unlock(&shared_skate_l.lock);
		}

		// calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);

		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);
		
		// aggiornamento dati di diagnostica
		pthread_mutex_lock(&sh_sensor_l_diag.lock);
		sh_sensor_l_diag.data.WCET = max(sh_sensor_l_diag.data.WCET, exec_time);
		sh_sensor_l_diag.data.avg_sensor = lastAvg;
		pthread_mutex_unlock(&sh_sensor_l_diag.lock);
	}

	/* Clear */
    if (mq_close(sensor_l_qd) == -1) {
        perror("acquire filter loop: mq_close sensor_l_qd");
        exit(1);
    }

	return 0;
}

void * control_loop_l(void * par) {

	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th, TICK_TIME);
	
	// Messaggio da prelevare dal reference
	char message[MAX_MSG_SIZE];
	
	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo la coda per il reference, in lettura e non bloccante
	mqd_t reference_l_qd;
	if ((reference_l_qd = mq_open(REFERENCE_L_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		// O_NONBLOCK serve perchè il task di controllo non si blocchi in attesa di un nuovo messaggio
		perror("control loop: mq_open (reference_l)");
		exit(1);
	}

	unsigned int reference_l = 110;

	unsigned int plantL_state = 0; 	// conserva il valore filtrato del sensore
	int error_l = 0;
	unsigned int control_action_l = 0;

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_ctrl_l_diag.lock);
	init_diag_data(&sh_ctrl_l_diag.data, "CTR L");
	pthread_mutex_unlock(&sh_ctrl_l_diag.lock);

	struct timespec t_start, t_end; 

	while (keep_on_running) {
		wait_next_activation(th);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

		// legge il plant state 
		pthread_mutex_lock(&shared_avg_sensor_l.lock);
		plantL_state = shared_avg_sensor_l.value;
		pthread_mutex_unlock(&shared_avg_sensor_l.lock);

		// riceve la reference (Coda non bloccante)
		if (mq_receive(reference_l_qd, message, MAX_MSG_SIZE, NULL) == -1){
			//printf ("No reference ...\n");	//DEBUG
		} else {
			reference_l = atoi(message);
		}

		// la control action è calcolata localmente affinchè la sezione critica non sia troppo lunga
		// se reference = 0, control action di frenata
		if (reference_l == 0) {
			control_action_l = 4;	// frenare

			// ABS con rilevazione nel sensore
			int skt_r, skt_l;
			pthread_mutex_lock(&shared_skate_r.lock);
			skt_r = shared_skate_r.value;
			pthread_mutex_unlock(&shared_skate_r.lock);
			pthread_mutex_lock(&shared_skate_l.lock);
			skt_l = shared_skate_l.value;
			pthread_mutex_unlock(&shared_skate_l.lock);
			if (skt_r || skt_l) {
				// se almeno una delle due ruote va in skating, allora deceleriamo invece di frenare
				control_action_l = 2;
			}
		} else { // calcolo della legge di controllo
			error_l = reference_l - plantL_state;
			if (error_l > 0) {
				control_action_l = 1;		// accelerare
			} else if (error_l < 0) {
				control_action_l = 2;		// decelerare
			} else {
				control_action_l = 3;		// non fare niente
			}
		}

		// aggiorna la control action
		pthread_mutex_lock(&shared_control_l.lock);
		shared_control_l.value = control_action_l;
		pthread_mutex_unlock(&shared_control_l.lock);
		
		// calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);

		// aggiorno i dati di diagnostica
		pthread_mutex_lock(&sh_ctrl_l_diag.lock);
		sh_ctrl_l_diag.data.WCET = max(sh_ctrl_l_diag.data.WCET, exec_time);
		sh_ctrl_l_diag.data.control_action = control_action_l;
		sh_ctrl_l_diag.data.reference = reference_l;
		pthread_mutex_unlock(&sh_ctrl_l_diag.lock);
	}

	/* Clear */
    if (mq_close(reference_l_qd) == -1) {
        perror("control loop: mq_close reference_l_qd");
        exit(1);
    }
	return 0;
}

void * actuator_loop_l(void * par) {

	periodic_thread *th = (periodic_thread *) par;
	start_periodic_timer(th, TICK_TIME);

	// Messaggio da prelevare dal driver
	char message_l[MAX_MSG_SIZE];

	/* Coda */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo la coda actuator del plant in scrittura 
	mqd_t actuator_l_qd;
	if ((actuator_l_qd = mq_open(ACTUATOR_L_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("actuator  loop: mq_open (actuator l)");
		exit(1);
	}

	// popolo la struct con i dati di diagnostica
	pthread_mutex_lock(&sh_actuator_l_diag.lock);
	init_diag_data(&sh_actuator_l_diag.data, "ACT L");
	pthread_mutex_unlock(&sh_actuator_l_diag.lock);

	struct timespec t_start, t_end;

	unsigned int control_action_l = 0;	// la control action è salvata in locale
	unsigned int control_l = 0;			// controllo inviato al plant
	while (keep_on_running) {
		wait_next_activation(th);
		// salvo il tempo di thread per valutare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

		// prelievo della control action dalla risorsa condivisa
		pthread_mutex_lock(&shared_control_l.lock);
		control_action_l = shared_control_l.value;
		pthread_mutex_unlock(&shared_control_l.lock);
		
		switch (control_action_l) {
			case 1:  control_l = 1;  break;		// accelerare
			case 2:	 control_l = -1; break;		// decelerare
			case 3:	 control_l = 0;  break;		// non fare niente
			case 4:  control_l = -2; break;		// frenare
			default: control_l = 0;
		}
		printf("Control L: %d\n", control_l);
		// le code comunicano via array di char
		sprintf(message_l, "%d", control_l); 
		// invio del controllo al driver del plant
		if (mq_send(actuator_l_qd, message_l, strlen(message_l) + 1, 0) == -1) {
		    perror("Sensor driver: Not able to send message to controller");
		    continue;
		}

		// calcolo del tempo finale per aggiornare il WCET
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
		// execution time per questa run
		unsigned long exec_time = difference_ns(&t_end, &t_start);
		// aggiorno i dati di diagnostica
		pthread_mutex_lock(&sh_actuator_l_diag.lock);
		sh_actuator_l_diag.data.WCET = max(sh_actuator_l_diag.data.WCET, exec_time);
		sh_actuator_l_diag.data.control = control_l;
		pthread_mutex_unlock(&sh_actuator_l_diag.lock);
	}

	/* Clear */
	if (mq_close (actuator_l_qd) == -1) {
        perror("Actuator loop: mq_close actuator_l_qd");
        exit(1);
    }
	return 0;
}

void * ps(void* parameter) {
	periodic_thread *th = (periodic_thread *) parameter;
	start_periodic_timer(th, 0);

	// Messaggio che verrà riveuto dal task diag 
	char in_buffer[MAX_DIAG_MSG_SIZE];
		
	/* Queues */
	struct mq_attr attr;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_DIAG_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo una coda in sola lettura (O_RDONLY), se non esiste la creiamo (O_CREAT). Inoltre la coda sarà non bloccante (O_NONBLOCK)
	// La coda riceverà le richieste aperiodiche
	static mqd_t qd_diag_request;
	if ((qd_diag_request = mq_open(DIAG_REQUEST_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("mq_open (diag_request)");
		exit(1);
	}

	// Apriamo una coda in sola scrittura (O_WRONLY)
	// La coda serve ad inviare messaggi al monitor per la stampa a video
	static mqd_t qd_diag_response;
	if ((qd_diag_response = mq_open(DIAG_RESPONSE_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror("mq_open (diag_response)");
		exit(1);
	}

	while(keep_on_running) {
		wait_next_activation(th);
		// messaggio di diagnostica da mandare
		char out_buffer[MAX_DIAG_MSG_SIZE];

		if (mq_receive(qd_diag_request, in_buffer, MAX_DIAG_MSG_SIZE, 0) == -1) {
			// perror("controller_ps (qd_diag_request)");
		} else {
			// costruiamo il messaggio di diagnostica concatendando i contenuti delle struct di diagnostica
			pthread_mutex_lock(&sh_sensor_r_diag.lock);
			append_to_string(&sh_sensor_r_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_sensor_r_diag.lock);

			pthread_mutex_lock(&sh_sensor_l_diag.lock);
			append_to_string(&sh_sensor_l_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_sensor_l_diag.lock);

			pthread_mutex_lock(&sh_ctrl_r_diag.lock);
			append_to_string(&sh_ctrl_r_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_ctrl_r_diag.lock);

			pthread_mutex_lock(&sh_ctrl_l_diag.lock);
			append_to_string(&sh_ctrl_l_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_ctrl_l_diag.lock);

			pthread_mutex_lock(&sh_actuator_r_diag.lock);
			append_to_string(&sh_actuator_r_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_actuator_r_diag.lock);

			pthread_mutex_lock(&sh_actuator_l_diag.lock);
			append_to_string(&sh_actuator_l_diag.data, out_buffer);
			pthread_mutex_unlock(&sh_actuator_l_diag.lock);

			// mando il messaggio sulla coda
            if (mq_send(qd_diag_response, out_buffer, strlen(out_buffer) + 1, 0) == -1) {
				perror("controller_ps (mq_send su qd_diag_response)");
			}

			// pulisco il buffer scrivendo il carattere di terminazione in prima posizione
			out_buffer[0] = '\0';
		}

	}	

}

int main(void) {
	printf("The controller is STARTED! [press 'q' to stop]\n");
 	
	// definizione threads
	pthread_t acquire_filter_r_thread;
    pthread_t control_r_thread;
    pthread_t actuator_r_thread;
	pthread_t acquire_filter_l_thread;
    pthread_t control_l_thread;
    pthread_t actuator_l_thread;
	pthread_t ps_thread;

	// inizializzazione mutex
	pthread_mutex_init(&shared_avg_sensor_r.lock, NULL);
	pthread_mutex_init(&shared_skate_r.lock, NULL);
	pthread_mutex_init(&shared_avg_sensor_l.lock, NULL);
	pthread_mutex_init(&shared_skate_l.lock, NULL);
	pthread_mutex_init(&shared_control_r.lock, NULL);
	pthread_mutex_init(&shared_control_l.lock, NULL);
	// non c'è priority inheritance (in questo caso specifico non può esserci inversione di prio)

	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT);
	// in questo caso più threads usano la stessa risorsa condivisa:
	// usiamo PI per evitare inversione di priorita'

	pthread_mutex_init(&sh_sensor_r_diag.lock, &mutex_attr);
	pthread_mutex_init(&sh_sensor_l_diag.lock, &mutex_attr);
	pthread_mutex_init(&sh_ctrl_r_diag.lock, &mutex_attr);
	pthread_mutex_init(&sh_ctrl_l_diag.lock, &mutex_attr);
	pthread_mutex_init(&sh_actuator_r_diag.lock, &mutex_attr);
	pthread_mutex_init(&sh_actuator_l_diag.lock, &mutex_attr);

	pthread_mutexattr_destroy(&mutex_attr);

	pthread_attr_t myattr;
	struct sched_param myparam;

	pthread_attr_init(&myattr);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO); // schedulazione con RM
	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED); 

	// ACQUIRE FILTER THREAD
	periodic_thread acquire_filter_th;
	acquire_filter_th.period = TICK_TIME;
	acquire_filter_th.priority = 50;

	myparam.sched_priority = acquire_filter_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&acquire_filter_r_thread, &myattr, acquire_filter_loop_r, (void*)&acquire_filter_th);
	pthread_create(&acquire_filter_l_thread, &myattr, acquire_filter_loop_l, (void*)&acquire_filter_th);

	// CONTROL THREAD
	periodic_thread control_th;
	control_th.period = TICK_TIME*BUF_SIZE;
	control_th.priority = 45;

	myparam.sched_priority = control_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&control_r_thread, &myattr, control_loop_r, (void*)&control_th);
	pthread_create(&control_l_thread, &myattr, control_loop_l, (void*)&control_th);

	// ACTUATOR THREAD
	periodic_thread actuator_th;
	actuator_th.period = TICK_TIME*BUF_SIZE;
	actuator_th.priority = 45;

	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&actuator_r_thread, &myattr, actuator_loop_r, (void*)&actuator_th);
	pthread_create(&actuator_l_thread, &myattr, actuator_loop_l, (void*)&actuator_th);


	// POLLING SERVER THREAD
	periodic_thread ps_th;
	ps_th.period = 10 * TICK_TIME;
	ps_th.priority = 30;	// PS ha priorita' piu' bassa perche' il suo periodo e' il maggiore tra tutti
	myparam.sched_priority = ps_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 	
	
	pthread_create(&ps_thread, &myattr, ps, &ps_th);

	// AFFINITY
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(acquire_filter_r_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(acquire_filter_l_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(control_r_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(control_l_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(actuator_r_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(actuator_l_thread, sizeof(cpuset), &cpuset);
	pthread_setaffinity_np(ps_thread, sizeof(cpuset), &cpuset);

	pthread_attr_destroy(&myattr);
	
	
	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') {
			break;
		}
  	}
	// interrompe i cicli di tutti i threads
	keep_on_running = 0;

	// UNLINK DELLE CODE 
	if (mq_unlink(REFERENCE_R_QUEUE_NAME) == -1) {
        perror("Main: mq_unlink reference_r queue");
        exit(1);
	}
	if (mq_unlink(REFERENCE_L_QUEUE_NAME) == -1) {
        perror("Main: mq_unlink reference_l queue");
        exit(1);
    }
	if (mq_unlink(DIAG_REQUEST_QUEUE_NAME) == -1) {
		perror("Main: mq_unlink diag_req queue");
		exit(1);
	}
	if (mq_unlink(DIAG_RESPONSE_QUEUE_NAME) == -1) {
		perror("Main: mq_unlink diag_res queue");
		exit(1);
	}

 	printf("The controller is STOPPED\n");
	return 0;
}




