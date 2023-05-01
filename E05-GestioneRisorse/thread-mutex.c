// FIFO Threads Priority Inversion simulation

#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

int shared;	//risorsa condivisa (variabile globale)
pthread_mutex_t mymutex;	//pthread_..._t= tipo

void * run_high(void * wcet) {
	
	int i = 1;
	int j;
	int a = 13, b = 17;
	usleep(1);
	printf("Thread High about to lock...\n");
	//simulating a long critical section 
	pthread_mutex_lock(&mymutex);	//lock del mutex
	while (i <= (long)wcet) {
		int sh = shared;
		for(j=0; j<100000; j++) a*=b;	// moltiplicazione tra primi, non può essere ottimizzata dal compilatore
		shared = sh+1;
		if (i%100==0) printf("Thread High, shared=%d\n",shared);
		i++;
	}
	printf("Thread High about to unlock...\n");
	pthread_mutex_unlock(&mymutex);
}

void * run_low(void * wcet) {
	
	int i = 1;
	int j;
	int a = 13, b = 17;
	printf("Thread Low about to lock...\n");
	//simulating a long critical section
	pthread_mutex_lock(&mymutex);	//lock del mutex

	while (i <= (long)wcet) {
		int sh = shared;
		for(j=0; j<100000; j++) a*=b;
		shared = sh+1;
		if (i%100==0) printf("Thread Low, shared=%d\n",shared);
		i++;
	}	
	printf("Thread Low about to unlock...\n");
	pthread_mutex_unlock(&mymutex);
}

void * run_medium(void * wcet) {	//non usa la risorsa!

	int i;
	int a = 13, b = 17;

	usleep(4);
	// cpu burn
	for(i=0; i<(long)wcet*100000; i++) {
		a *= b;
		if (i%10000000==0) printf("Thread Medium, CPU BURN!\n");
	}
	
}


int main(){

	pthread_t threads[3]; 

	//init mutex attr 
	pthread_mutexattr_t mymutexattr;
	pthread_mutexattr_init(&mymutexattr);
	//pthread_mutexattr_setprotocol(&mymutexattr,PTHREAD_PRIO_INHERIT); //Priority Inheritance (ovviamente non richiede il ceiling)
	pthread_mutexattr_setprotocol(&mymutexattr,PTHREAD_PRIO_PROTECT); //Priority Ceiling
	pthread_mutexattr_setprioceiling(&mymutexattr,20); //ceiling: priorita' del task a prio + alta che usa la risorsa
	pthread_mutex_init(&mymutex,&mymutexattr);	//inizializzazione mutex 
  	
  	// init thread attr
  	struct sched_param myparam;
	pthread_attr_t myattr;
	pthread_attr_init(&myattr);
    pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
    pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED);
	
  	// start threads	threads aperiodici 
	myparam.sched_priority = 10;	//priorita' bassa 
    pthread_attr_setschedparam(&myattr, &myparam); 
    pthread_create(&threads[0], &myattr, run_low, (void*)20000);
    
    myparam.sched_priority = 20;	//priorita' alta 
    pthread_attr_setschedparam(&myattr, &myparam); 
    pthread_create(&threads[1], &myattr, run_high, (void*)5000);
   
    
    myparam.sched_priority = 15;	//priorita' media: genera Priority Inversion
    pthread_attr_setschedparam(&myattr, &myparam); 
    pthread_create(&threads[2], &myattr, run_medium, (void*)500); 
    
    /*i primi due threads condividono la risorsa shared*/
    
    
 	// wait threads
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[2], NULL);
		
	pthread_attr_destroy(&myattr);
	pthread_mutexattr_destroy(&mymutexattr);	//distruzione attributo mutex
	pthread_mutex_destroy(&mymutex);			//distruzione mutex 
	

	printf("Final shared value: %d\n",shared);

    return 0;
}

/* E' implementato un PC immediato (Immediate PC): appena entro nella sezione critica innalzo 
la priorita' del processo: me ne accorgo durante l'esecuzione (evito i deadlock 
ma non le catene di bloccaggio)*/

