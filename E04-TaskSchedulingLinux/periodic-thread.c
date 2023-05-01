// programma base da usare sempre 

#include <sys/time.h> 	//funzioni tempo linux
#include <time.h> 	//funzioni tempo POSIX (clock nanosleep)
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

/* mentre prima  le funzioni wait_next_activation e start_periodic_timer 
lavoravano su variabili globali (periodo del task e tempo in cui viene 
programmata cla clock_nanosleep, in cui il task dorme per un periodo), ora 
lavoriamo con una struttura che contiene tutti gli attributi del thread
Questa struttura verrà passata alle funzioni, per non usare le variabili globali*/

struct periodic_thread {
	int index;	//diverso dal PID ma può servire ad identificare il thread
	struct timespec r;	// istante di tempo in cui termina il periodo
	int period;		// durata del periodo in us
	int wcet;	//utile per il test di schedulabulità
	int priority;
};

#define NSEC_PER_SEC 1000000000ULL

//funzione che aggiunge un tempo in microsecondi ad una timespec
static inline void timespec_add_us(struct timespec *t, uint64_t d)	{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
}

void wait_next_activation(struct periodic_thread * thd)	{ //passo un puntatore alla struttura 
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &(thd->r), NULL);
    timespec_add_us(&(thd->r), thd->period);
}

void start_periodic_timer(struct periodic_thread * thd, uint64_t offs) {
    clock_gettime(CLOCK_MONOTONIC, &(thd->r));	//per ogni thread aggiorno la SUA copia del parametro (?)
    timespec_add_us(&(thd->r), offs);			//aggiorno il periodo di ogni thread 
}

void * run (void * par) {				//starting routine del thread 
	struct periodic_thread *th = (struct periodic_thread *) par;	//struttura inizializzata nel main 
	
	start_periodic_timer(th,2000000);		//imposto una fase di 2 secondi 
    
    int cnt = 0;
    uint64_t start = 0;
    uint64_t t;
    struct timeval tv;
    
    while(1) {
        wait_next_activation(th);
        if (start == 0) {
        	gettimeofday(&tv, NULL);
			start = tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;	// converto l'ora attuale in us
    	}
        
    	gettimeofday(&tv, NULL);
    	t = tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
    	if (cnt && (cnt % 100) == 0) {
        	printf("th %d: Avg time: %f\n", th->index, (double)(t - start) / (double)cnt);
    	}
    	cnt++;
    } //job body con comportamento ciclico direttamente all'interno del thread
}


//usiamo il main per l'inizializzazione 
int main()
{
	pthread_t thread_1;

	struct periodic_thread th1;
	th1.index = 1;
	th1.period = 5000;
	
    pthread_create(&thread_1, NULL, run, &th1); //il thread esegue la funzione run, posso fare il cast 
	pthread_t thread_2;

	struct periodic_thread th2;
	th2.index = 2;
	th2.period = 10000;
   	 pthread_create(&thread_2, NULL, run, &th2);
	while (1) {
   		if (getchar() == 'q') break;
  	}

    return 0; //non uso pthread_exit, quando esco dal main termino automaticamente il thread 
}

