/* $RCSfile: perf_spy.c,v $ (version $Revision$)
 * $Date: 1995/12/27 11:45:08 $, .
 *
 * functions to spy performances. 
 * I agree, -pg does a better job, but for page fault and related issues...
 */

#include <stdio.h>
#include <stdlib.h>

/* for USAGE information
 */
#include <sys/time.h>
#include <sys/resource.h>
extern int getrusage(int, struct rusage*); /* not found in any header! */

#include "genC.h"
#include "misc.h"

#define USAGE_STACK_SIZE 10
static struct rusage usage_stack[USAGE_STACK_SIZE];
static int index = 0; /* available bucket */

static void
printf_time(
    FILE *f,
    struct timeval *p)
{
    fprintf(f, "%.3f", (double) p->tv_sec + (double) p->tv_usec/1e6);
}

static void 
printf_time_delta(
    FILE *f,
    struct timeval *begin,
    struct timeval *end)
{
    struct timeval delta;
    delta.tv_sec  = end->tv_sec - begin->tv_sec;
    delta.tv_usec = end->tv_usec - begin->tv_usec;
    printf_time(f, &delta);
}

/* simply dump all available information...
 */
static void
printf_usage_delta(
    FILE *f,
    struct rusage *begin,
    struct rusage *end)
{
    fprintf(f, " user: "); 
    printf_time_delta(f, &begin->ru_utime, &end->ru_utime);
    fprintf(f, "/");
    printf_time(f, &end->ru_utime);
    fprintf(f, "s syst: "); 
    printf_time_delta(f, &begin->ru_stime, &end->ru_stime);
    fprintf(f, "/");
    printf_time(f, &end->ru_stime);
    fprintf(f, "s\n page faults major: %ld/%ld minor: %ld/%ld\n",
	    end->ru_majflt-begin->ru_majflt, end->ru_majflt,
	    end->ru_minflt-begin->ru_minflt, end->ru_minflt);
}

void 
push_performance_spy()
{
    message_assert("stack not full", index<USAGE_STACK_SIZE);
    getrusage(RUSAGE_CHILDREN, &usage_stack[index++]);
    getrusage(RUSAGE_SELF, &usage_stack[index++]);
}

void 
pop_performance_spy(
    FILE *f,
    string msg)
{
    struct rusage current;
    message_assert("stack not empty", index>0);
    fprintf(f, "[performance_spy] %s\n", msg);

    getrusage(RUSAGE_SELF, &current);
    fprintf(f, "self:\n");
    printf_usage_delta(f, &usage_stack[--index], &current);
    fprintf(f, "children:\n");
    getrusage(RUSAGE_CHILDREN, &current);
    printf_usage_delta(f, &usage_stack[--index], &current);
}

