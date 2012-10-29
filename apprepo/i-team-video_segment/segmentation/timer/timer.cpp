#include "timer.hpp"
/* Return the amount of time in useconds used by the current process since it began. */
long long user_time() {
    struct timeval tv;
    gettimeofday(&tv, (struct timezone*) NULL);
    return ((tv.tv_sec * 1000000) + (tv.tv_usec));   // usec
}


/* Starts timer. */
void start_timer(int timer) {
    timer_set[timer] = TRUE;
    old_time[timer] = user_time();
}


/* Returns elapsed time since last call to start_timer().
   Returns ERROR_VALUE if Start_Timer() has never been called. */
double  elapsed_time(int timer) {
    if (timer_set[timer] != TRUE) {
        return (ERROR_VALUE);
    } else {
        return (user_time() - old_time[timer]) / 1000.0  ; // msec
    }
}
