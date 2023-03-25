/*
PARALLELISM:
    Two processes or threads are doing work at exactly the same time.
    It typically requires some kind of hardware support - multiple cores or coprocessor

    PROBLEMS:
        1. Shared memory! (The OS may however stick those processes on the same processor to avoid this, but the point is that sharing may prevnt from getting parallelism)

    SOLUTIONS:
        1. - Locks. 
           - Make use of ATOMIC operations. Use of compiler provided ATOMIC operations is not recommended as they may not be portable.

CONCURRENCY:
    This can be thought of as the case when you only have one processor but you want multiple processes or threads to progress at the same time. We know that they cannot be literally progressing at the same time on the same processor thus, resources are shared simultaneously among different processes thus allowing different processes to progress simultaneously, creating an illusion of concurrency.
    One process has to wait till the other process finishes(releases the resources).

*/

// The following code is NOT THREAD SAFE. It is because variable is being shared among threads without locking. This may also be called race condition => a condition where threads compete to get the resource first and output of the program depends on which thread gets there sooner! 

// CORRECTNESS IS MORE IMPORTANT THAN SPEED!

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#define BIG 100000000UL
uint32_t counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;       // mutex lock declared globally

void *count_to_big(void *arg)
{
    for (uint32_t i = 0; i < BIG; i++)
    {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, count_to_big, NULL);

    count_to_big(NULL);
    pthread_join(t, NULL);
    printf("Done! Counter = %u`\n", counter);
}