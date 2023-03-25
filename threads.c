#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *fun1(void *arg)       //this becomes the function pointer (start point of thread)
{
    int *iptr = (int *)arg;
    for(int i=0; i<8; i++)
    {
        sleep(1);
        printf("fun1 : %d\t v : %d\n", i, *iptr);
        (*iptr)++;
    }
    return NULL;
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
    int v = 5;                  // argument

    pthread_create(&newthread, NULL, fun1, &v);      // create a new thread and give a start routine
    // fun1();
    function2();            // this now becomes the second thread
    pthread_join(newthread, NULL);
    printf("thread's done! v : %d\n", v);

    return 0;
}

/*
At this point, the program terminates once one of the thread completes its execution. To avoid this, we join the threads using pthread_join()
*/