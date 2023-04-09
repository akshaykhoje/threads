#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *routine() {
    printf("Child thread\n");
    sleep(1);
    printf("Ending thread\n");
    return NULL;
}

int main(void) {
    
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, routine, NULL) != 0) {  // error checking conditions
        return 1;
    }
    if (pthread_create(&t2, NULL, routine, NULL)!= 0) {
        return 2;
    }
    printf("Parent thread!\n");
    if (pthread_join(t1, NULL)!= 0) {
        return 3;
    }
    if (pthread_join(t2, NULL)!= 0) {
        return 4;
    }
    return 0;
}