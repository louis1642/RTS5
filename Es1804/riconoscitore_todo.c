#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MIN_SEMIPERIOD 10000			//10ms
#define NSEC_PER_SEC 1000000000ULL
#define NGENERATORS 3

#define SEQUENCELENGTH 4
const int sequence[SEQUENCELENGTH] = {0, 3, 6, 5};
int r_state;

pthread_mutex_t seq_mutex;	// proteggo la seq_str
pthread_mutex_t r_mutex; 	// proteggo la recognizer_data_str

/************************** DATA STRUCTURES *******************************/

/* Sequence data structure */
struct seq_str {
	unsigned int bit[NGENERATORS];	//Data produced by the generators
	// in realtà sarebbe più chiaro dichiarare la mutex nella struct
	// pthread_mutex_t seq_mutex;
};		
static struct seq_str seq_data;		// static: la modifica fatta su un thread viene vista da tutti i thread

// struttura dati condivisa e protetta contentente le variabili "count" e "ok" usate dal recognizer
struct recognizer_data_str {
	int ok;
	int count;
};
static struct recognizer_data_str r_data;

/* periodic thread */
struct periodic_thread {
	int index;
	struct timespec r;
	int period;
	int phase;		// aggiunta la fase alla struct solita
	int wcet;
	int priority;
};


/***************************** UTILITY FUNCTIONS ***********************************/

static inline void timespec_add_us(struct timespec *t, uint64_t d)
{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
}

void wait_next_activation(struct periodic_thread * thd)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(thd->r), NULL);
    timespec_add_us(&(thd->r), thd->period);
}

void start_periodic_timer(struct periodic_thread * thd, uint64_t offs)
{
    clock_gettime(CLOCK_REALTIME, &(thd->r));
    timespec_add_us(&(thd->r), offs);
}

// converte i tre bit nella struct in un numero intero decimale
static inline int b2d(struct seq_str* seq) {
	return seq->bit[0] + seq->bit[1] * 2 + seq->bit[2] * 4;
}

/***************************** THREADS ************************************/

void* rt_generator_thread(void* parameter) {
	struct periodic_thread * th = (struct periodic_thread *) parameter;
	printf("Start Generator %d\n",th->index);	//DEBUG
	
	start_periodic_timer(th,th->phase);
	while (1) {
		wait_next_activation(th);
		//accedere alle variabili condivise in mutua esclusione
		pthread_mutex_lock(&seq_mutex);
		// se avessi definito la mutex nella struct,
		// pthread_mutex_lock(&seq_data.mutex);
		if (seq_data.bit[th->index]) {
			seq_data.bit[th->index] = 0;
		} else{ 
			seq_data.bit[th->index] = 1;
		}
		pthread_mutex_unlock(&seq_mutex);
	}
}

void* rt_recognizer_thread(void* parameter){
	
	//Implementare un riconoscitore di sequenza che 
	//ad ogni periodo verifica se gli ultimi quattro valori letti corrispondono alla sequenza 0,3,6,5
	struct periodic_thread* th = (struct periodic_thread*) parameter;
	printf("Start recognizer (index = %d)\n",th->index);
	// inizializzo lo stato del riconoscitore
	r_state = 0;
	start_periodic_timer(th, th->phase);
	while (1) {
		wait_next_activation(th);
		// sezione critica: accesso dati seq
		pthread_mutex_lock(&seq_mutex);
		if (b2d(&seq_data) == sequence[r_state]) {
			++r_state;
			if (r_state == SEQUENCELENGTH) {
				// intera sequenza riconosciuta
				// sezione critica (innestata): accesso dati recognizer
				pthread_mutex_lock(&r_mutex);
				r_data.ok = 1;
				++(r_data.count);
				pthread_mutex_unlock(&r_mutex);
			}
		} else {	// non è la sequenza: resetto lo stato
			// manca la condizione in cui sbaglio ma è uguale a sequence[0] -> r_state = 1
			r_state = 0;
		}
		pthread_mutex_unlock(&seq_mutex);
	}
}

void* nrt_buddy_thread(void* parameter){
	struct periodic_thread *th = (struct periodic_thread *) parameter;
	printf("Start Buddy (index = %d)\n",th->index);
	start_periodic_timer(th,th->phase);
	int i = 0;

	while (1) {
		wait_next_activation(th);
		
		//inserire mutua esclusione per l'accesso alle variabili condivise 		
		pthread_mutex_lock(&seq_mutex);
		printf("bits : ");			//DEBUG
		for(i=NGENERATORS-1; i>-1; i--){
			printf("%d ",seq_data.bit[i]);
		}
		printf("\t (decimal: %i)\n", b2d(&seq_data));
		pthread_mutex_unlock(&seq_mutex);

		//Stampa dei valori "count" e "ok" prodotti dal recognizer
		pthread_mutex_lock(&r_mutex);
		if (r_data.ok == 1) {
			r_data.ok = 0;
			printf("OK! count = %i\n", r_data.count);
		}
		pthread_mutex_unlock(&r_mutex);
	}

}


/***************************** MAIN **************************************/

int main() {	
	
	// Modificare qui per scegliere il periodo e la fase dei thread 
	int arraySemiperiods[NGENERATORS] = {1, 2, 2};
	int arrayPhases[NGENERATORS] = {1, 1, 2};
	int i = 0;

	/* Initialize the shared data*/
	for(i=0; i<NGENERATORS; i++){
		seq_data.bit[i] = 0;
	}

	struct periodic_thread th_g[NGENERATORS];
	struct periodic_thread th_r;
	struct periodic_thread th_buddy;

	pthread_t thread_generator[NGENERATORS];
	pthread_t thread_recognizer;
	pthread_t thread_buddy;

	pthread_attr_t thread_sch_attrib;
	struct sched_param thread_sch_param;

	pthread_attr_init(&thread_sch_attrib);
	pthread_attr_setschedpolicy(&thread_sch_attrib, SCHED_FIFO);
	pthread_attr_setinheritsched(&thread_sch_attrib, PTHREAD_EXPLICIT_SCHED);
	// se non setto PTHREAD_EXPLICIT_SCHED i figli prenderanno lo scheduling del padre
	
	//Inizializzare i Mutex per le variabili condivise
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_setprotocol(&mutexattr,PTHREAD_PRIO_PROTECT);
	pthread_mutexattr_setprioceiling(&mutexattr, 13l);	// setto il ceiling
	pthread_mutex_init(&seq_mutex, &mutexattr);		// inizializzo il seq_mutex con gli attributi appena settati
	pthread_mutex_init(&r_mutex, &mutexattr);




	/* Generator threads*/
	for(i=0; i<NGENERATORS; i++){
		/*periodic thread initialization*/
		th_g[i].index = i;
		th_g[i].period = arraySemiperiods[i]*MIN_SEMIPERIOD;
		th_g[i].phase = arrayPhases[i]*MIN_SEMIPERIOD;
		th_g[i].priority = 12 - i;
		
		//Inizializza gli attributi dei threadi per renderli real-time
		//I thread devono essere schedulati con Rate Monotonic
		// gli attributi comuni sono assegnati fuori dal ciclo for
		thread_sch_param.sched_priority = th_g[i].priority;
		pthread_attr_setschedparam(&thread_sch_attrib, &thread_sch_param);

		/*thread creation*/
		pthread_create(&thread_generator[i], &thread_sch_attrib, rt_generator_thread, &th_g[i]);  
	}



	//Inizializza e crea il thread hard real-time recognizer 
	th_r.index = NGENERATORS + 1;
	th_r.period = 1*MIN_SEMIPERIOD;
	th_r.phase = 0;
	th_r.priority = 13;
	// riuso thread_sch_attrib
	thread_sch_param.sched_priority = th_r.priority;
	pthread_attr_setschedparam(&thread_sch_attrib, &thread_sch_param);
	pthread_create(&thread_recognizer, &thread_sch_attrib, rt_recognizer_thread, &th_r);


	/* buddy thread */
	th_buddy.index = NGENERATORS + 2;
	th_buddy.period = MIN_SEMIPERIOD; //10 ms
	th_buddy.phase =  MIN_SEMIPERIOD; //10 ms
	/*thread creation*/	
	pthread_create(&thread_buddy,NULL,nrt_buddy_thread,&th_buddy);


	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') break;
  	}
  	
  	/* Clear */
	for(i=0; i<NGENERATORS; i++){
  		pthread_kill(thread_generator[i],0);
	}
	pthread_kill(thread_recognizer,0);
	pthread_kill(thread_buddy,0);

	// Pulisci gli attributi usati per inizializzare i thread e i seq_mutex
	pthread_attr_destroy(&thread_sch_attrib);
	pthread_mutex_destroy(&seq_mutex);
	pthread_mutex_destroy(&r_mutex);
	pthread_mutexattr_destroy(&mutexattr);
	

  	printf("EXIT!\n");

	return 0;
}
