#ifndef __RTLIB
#define __RTLIB
#include <time.h>
#define NSEC_PER_SEC 1000000000ULL

/* periodic thread */
typedef struct{
	int index;
	struct timespec r;
	int period;
	int wcet;
	int priority;
} periodic_thread;

/* time utility functions */

void timespec_add_us(struct timespec *t, unsigned long d);

unsigned long int difference_ns(struct timespec *ts1, struct timespec *ts2);

/* return 1 if t1>t2, 0 otherwise*/
int compare_time(struct timespec *t1,struct timespec *t2);

void busy_sleep(int us);

/* periodic threads functions */
void wait_next_activation(periodic_thread * thd);

void start_periodic_timer(periodic_thread * thd, unsigned long offs);

#endif