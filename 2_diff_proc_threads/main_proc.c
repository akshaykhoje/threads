#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(void) {
    int x = 2;
    int pid;
    if ((pid = fork()) == -1) {
        return -1;
    }

    if(pid == 0) {
        x++;
    }
    sleep(2);
    printf("x : %d\n", x);

    printf("Process id : %d\n", getpid());
    if (pid == 0) {
        wait(NULL);
    }

    return 0;
}