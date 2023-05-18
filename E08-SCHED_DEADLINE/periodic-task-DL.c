#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include "sched_attributes.h"

static struct timespec r;
static int period;

#define NSEC_PER_SEC 1000000000ULL
static inline void timespec_add_us(struct timespec *t, uint64_t d)
{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
}

void wait_next_activation(void)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &r, NULL);
    timespec_add_us(&r, period);
}

void start_periodic_timer(uint64_t offs, int t)
{
    clock_gettime(CLOCK_REALTIME, &r);
    timespec_add_us(&r, offs);
    period = t;
}

static void job_body(void)
{
    static int cnt;
    static uint64_t start;
    uint64_t t;
    struct timeval tv;

    if (start == 0) {
        gettimeofday(&tv, NULL);
	start = tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
    }
        
    gettimeofday(&tv, NULL);
    t = tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
    if (cnt && (cnt % 100) == 0) {
        printf("Avg time: %f\n", (double)(t - start) / (double)cnt);
    }
    cnt++;
}

int main()
{   
    struct sched_attr attr;
	sched_getattr(0, &attr, sizeof(attr), 0);

	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = 500 * 1000; //ns	
	attr.sched_deadline = 5 * 1000 * 1000; //ns
	attr.sched_period = 5 * 1000 * 1000; //ns
	sched_setattr(0, &attr, 0);

    start_periodic_timer(2000000, 5000);
    
    while(1) {
        wait_next_activation();
        job_body();
    }

    return 0;
}
