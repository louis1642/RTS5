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

#define PERIOD_US 100000
#define NUM_TASKS 15
#define NSEC_PER_SEC 1000000000ULL
#define PS_QUEUE_NAME   "/ps_queue"
#define MONITOR_QUEUE_NAME "/monitor_queue"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256

/* periodic thread */
struct periodic_thread {
	int index;
	struct timespec r;
	int period;
	int wcet;
	int priority;
};
 
/* Queues */
static mqd_t qd_ps;
static mqd_t qd_monitor;

/***************************** Functions ************************************/

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

unsigned long int difference_ns(struct timespec *ts1, struct timespec *ts2){
	long int diff_sec, diff_nsec;
	diff_sec =(ts1->tv_sec - ts2->tv_sec);
	diff_nsec = (ts1->tv_nsec - ts2->tv_nsec);
	return diff_sec*NSEC_PER_SEC + diff_nsec;
}

/* return 1 if t1>t2, 0 otherwise*/
int compare_time(struct timespec *t1,struct timespec *t2){
    if(t1->tv_sec > t2->tv_sec){
        return 1;
    }
    else if(t1->tv_sec == t2->tv_sec && t1->tv_nsec > t2->tv_nsec){
        return 1;
    }
    else{
        return 0;
    }
}

void init_periodic_threads(struct periodic_thread *th){
	int i = 0;
	// Inizializzo il periodo, il wcet e la priorità dei threads
	
	for(i=0;i<5;i++){
		th[i].index = i;
		th[i].wcet = 4*PERIOD_US; 
		th[i].period = 100*PERIOD_US;
		th[i].priority = 1;
	}

	for(i=5;i<9;i++){
		th[i].index = i;
		th[i].wcet = 2*PERIOD_US; 
		th[i].period = 50*PERIOD_US;
		th[i].priority = 2;
	}

	for(i=9;i<13;i++){
		th[i].index = i;
		th[i].wcet = 1*PERIOD_US; 
		th[i].period = 30*PERIOD_US;
		th[i].priority = 4;
	}

	for(i=13;i<15;i++){
		th[i].index = i;
		th[i].wcet = 2*PERIOD_US; 
		th[i].period = 40*PERIOD_US;
		th[i].priority = 3;
	}
}

void busy_sleep(int us){
	int ret=0;
	struct timespec start;
	struct timespec end;
	struct timespec now;
	
	ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	if(ret == -1){
		printf("ERROR: busy_wait %d\n",getpid());
	}
	end = start;
	timespec_add_us(&end,us);
	
	//Continua finché END(tempo di fine) è maggiore di now
	do{
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
	}while(compare_time(&end,&now)); 
	
	/*
	printf("END BUSY WAIT:\n START\ts:%ld, ns:%ld \n NOW\ts:%ld, ns:%ld \n END\ts:%ld, ns:%ld \n"	  //DEBUG
		,start.tv_sec,start.tv_nsec,now.tv_sec,now.tv_nsec,end.tv_sec,end.tv_nsec);			  //DEBUG
	*/
	
}

int aperiodic_fun(int wcet, mqd_t *qd_monitor, char * message,int priority){
	
	//Invia un messaggio al processo monitor per fare la print
	if (mq_send (*qd_monitor, message, strlen (message) + 1, priority) == -1) {
		perror ("aperiodic_fun: Not able to send message to monitor");
		return -1;
	}
	busy_sleep(wcet);
	return 0;
}

/***************************** THREADS ************************************/


void* ps(void* parameter){
	struct periodic_thread *th = (struct periodic_thread *) parameter;
	start_periodic_timer(th,0);
	
	// Messaggio da inviare al monitor
	char message [] = "Richiesta aperiodica!";
	// Messaggio che verrà riveuto da rt_async 
	char in_buffer [MAX_MSG_SIZE];
		
	/* Queues */
	struct mq_attr attr;

	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;
	
	// Apriamo una coda in sola lettura (O_RDONLY), se non esiste la creiamo (O_CREAT). Inoltre la coda sarà non bloccante (O_NONBLOCK)
	// La coda riceverà le richieste aperiodiche
	if ((qd_ps = mq_open (PS_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("Server: mq_open (server)");
		exit (1);
	}
	
	// Apriamo una coda in sola scrittura (O_WRONLY)
	// La coda serve ad inviare messaggi al monitor per la stampa a video
	if ((qd_monitor = mq_open (MONITOR_QUEUE_NAME, O_WRONLY)) == -1) {
		perror ("Client: mq_open (server)");
		exit (1);
	}

	while (1) {
		wait_next_activation(th);
		printf("[%d] POLLING SERVER with period:%d\t priority:%d\t wcet:%d\n",th->index,th->period,th->priority,th->wcet);	//DEBUG
		
		if (mq_receive(qd_ps, in_buffer,MAX_MSG_SIZE,NULL) == -1){
			//printf ("No message ...\n");							//DEBUG
		}
		else{
			//printf ("Polling Server: message received: %s.\n",in_buffer);			//DEBUG
			aperiodic_fun(th->wcet,&qd_monitor,message,th->priority);
		}
	}
		
}

void* periodic_fun(void* parameter){
	struct periodic_thread *th = (struct periodic_thread *) parameter;
	start_periodic_timer(th,th->index*PERIOD_US);
	
	while (1) {
		wait_next_activation(th);
		printf("[%d] Thread with period:%d\t priority:%d\t wcet:%d\n",th->index,th->period,th->priority,th->wcet);		//DEBUG
		busy_sleep(th->wcet);
	}
}



int main(void){
	int i = 0;
	
	/* Threads */
	struct periodic_thread th[15];
	pthread_t thread[15];
	
	pthread_attr_t myattr;
	struct sched_param myparam;
	
	init_periodic_threads(th);
	
	pthread_attr_init(&myattr);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED); 
	
	for(i = 0; i < NUM_TASKS; i++){
		myparam.sched_priority = th[i].priority;
		pthread_attr_setschedparam(&myattr, &myparam); 
		
		pthread_create(&thread[i], &myattr, periodic_fun, &th[i]);
	}
	
	/* thread_ps*/
	struct periodic_thread th_ps;
	pthread_t thread_ps;
	
	th_ps.index = i;
	th_ps.wcet = 1*PERIOD_US; 
	th_ps.period = 10*PERIOD_US;
	th_ps.priority = 5;	
	myparam.sched_priority = th_ps.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 	
	
	pthread_create(&thread_ps, &myattr, ps, &th_ps);

	pthread_attr_destroy(&myattr);
	
	
	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') break;
  	}

	// Invio un messaggio di terminazione al monitor
	if (mq_send (qd_monitor, "q", sizeof(char) + 1, 0) == -1) {
		perror ("rt_ps: Not able to send message to server");
	}
  		
	/* Clean */
	for(i = 0; i < NUM_TASKS; i++){
		pthread_kill(thread[i],0);
	}
  	
  	pthread_kill(thread_ps,0);
  	
	if (mq_close (qd_monitor) == -1) {
		perror ("rt_ps: mq_close qd_monitor");
		exit (1);
	}

	if (mq_close (qd_ps) == -1) {
		perror ("rt_ps: mq_close qd_ps");
		exit (1);
	}

	if (mq_unlink (PS_QUEUE_NAME) == -1) {
		perror ("rt_ps: mq_unlink ps queue");
		exit (1);
	}

  	printf("EXIT!\n");

	return 0;
}
