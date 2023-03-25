#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

void *fun1(void *arg)       //this becomes the function pointer (start point of thread)
{
    int *iptr = (int *)malloc(sizeof(int));
    for(int i=0; i<8; i++)
    {
        sleep(1);
        printf("fun1 : %d\t v : %d\n", i, *iptr);
        (*iptr)++;
    }
    return iptr;
}

void function2()
{
    for(int i=0; i<3; i++)
    {
        sleep(2);
        printf("function2 : %d\n", i);
    }
    return;
}

int main(void)
{
    pthread_t newthread;        // create a new thread variable
    // int v = 5;                  // argument
    int *result;

    // pthread_create(&newthread, NULL, fun1, &v);      // create a new thread and give a start routine
    pthread_create(&newthread, NULL, fun1, NULL);
    // fun1();
    function2();            // this now becomes the second thread
    // pthread_join(newthread, NULL);                      // this function gets us the return value from the function
    pthread_join(newthread, (void *)&result);
    printf("thread's done! v : *result = %d\n", *result);

    return 0;
}

/*
At this point, the program terminates once one of the thread completes its execution. To avoid this, we join the threads using pthread_join()
*/