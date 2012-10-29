#ifndef TIMER_H_
#define TIMER_H_

#include <sys/time.h>
#include <sys/resource.h>
#include <cstddef>

#define ERROR_VALUE -1.0
#define FALSE 0
#define TRUE  1
#define MAX_TIMERS 10

static int timer_set[MAX_TIMERS];
static long long old_time[MAX_TIMERS];


/* Return the amount of time in useconds used by the current process since it began. */
long long user_time();

/* Starts timer. */
void start_timer(int timer);


/* Returns elapsed time since last call to start_timer().
   Returns ERROR_VALUE if Start_Timer() has never been called. */
double  elapsed_time(int timer);
#endif /*TIMER_H_*/



