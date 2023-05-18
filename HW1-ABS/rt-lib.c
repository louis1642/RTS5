//------------------- RT-LIB.C ---------------------- 

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include "rt-lib.h"

void timespec_add_us(struct timespec *t, unsigned long d)
{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
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

void wait_next_activation(periodic_thread * thd)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(thd->r), NULL);
    timespec_add_us(&(thd->r), thd->period);
}

void start_periodic_timer(periodic_thread * thd, unsigned long offs)
{
    clock_gettime(CLOCK_REALTIME, &(thd->r));
    timespec_add_us(&(thd->r), offs);
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

unsigned long int max(unsigned long int x, unsigned long int y) {
	if (x > y) {
		return x;
	} else {
		return y;
	}
}