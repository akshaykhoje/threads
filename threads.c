#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *fun1(void *arg)       //this becomes the function pointer (start point of thread)
{
    while (1)
    {
        sleep(1);
        printf("fun1\n");
    }
    return NULL;
}

void function2()
{
    while (1)
    {
        sleep(2);
        printf("function2\n");
    }
    return;
}

int main(void)
{
    pthread_t newthread;        // create a new thread variable
    pthread_create(&newthread, NULL, fun1, NULL);      // create a new thread and give a start routine
    // fun1();
    function2();            // this now becomes the second thread
    return 0;
}