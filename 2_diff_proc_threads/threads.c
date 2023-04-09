#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

int x = 2;

void *routine() {
    x+=4;
    sleep(2);
    printf("Process id %d\n", getpid());
    printf("x %d\n", x);
    return NULL;
}

void *routine2() {
    x++;
    sleep(2);
    printf("Process id %d\n", getpid());
    printf("x %d\n", x);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, &routine, NULL) != 0) {  // error checking conditions
        return 1;
    }
    if (pthread_create(&t2, NULL, &routine2, NULL)!= 0) {
        return 2;
    }
    if (pthread_join(t1, NULL)!= 0) {
        return 3;
    }
    if (pthread_join(t2, NULL)!= 0) {
        return 4;
    }
    printf("x %d\n", x);

    return 0;
}