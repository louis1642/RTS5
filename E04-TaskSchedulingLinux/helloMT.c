#include <pthread.h> //libreria POSIX che contiene le funzioni per i threads
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_THREADS	10

void* PrintHello(void* threadid) { //starting routine che stiamo usando 
   sleep(100);
   //sleep((long)threadid); //ogni thread va in pasa per threadid secondi--> saranno eseguiti in sequenza , levando la sleep il main esegue in concorrenza con i threads 
  
   printf("\n%ld: Hello World! \n", (long)threadid); //casting inverso  //long* &theadid 
   pthread_exit(NULL); //tipicamente usata con null, a meno che non voglio passare qualcosa per una join 
}

int main(int argc, char *argv[]) {
	pthread_t threads[NUM_THREADS]; //voglio creare 10 threads 
	int rc;
	long t;
	
	//creazione attributo come joinable 
	// un attributo (oggetto pthread_attr_t) contiene informazioni sul thread,
	//  riguardo ad esempio com'è schedulato
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	// rendiamo il thread joinable (inutile, già fatto da pthread_attr_init() )
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	
	for(t=0;t<NUM_THREADS;t++){

		printf("Creating thread %ld\n", t);

		//creazione del thread e casting	(void*)& threadid 
		//int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
        //                void *(*start_routine) (void *), void *arg);
		rc = pthread_create(&threads[t], &attr, PrintHello, (void *)t); 	

		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	
	for(t=0;t<NUM_THREADS;t++) {	//join
		// The pthread_join() function waits for the thread specified by thread to
       	// terminate.  If that thread has already terminated, then  pthread_join()
       	// returns immediately.  The thread specified by thread must be joinable.
		pthread_join(threads[t],NULL);
	}
	
	// distruggo l'oggetto attributo
	pthread_attr_destroy(&attr);

	// termino il programma (return o pthread_exit sono analoghi perchè ho fatto il join)
	pthread_exit(NULL);
}

