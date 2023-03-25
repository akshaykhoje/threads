/*
SEMAPHORES are used in synchronization which help co-ordinating activity between multiple concurrently running threads or processes.

It is basically an unsigned integer. Changes to it are atomic i.e. two operations cannot interrupt this variable at the same time.
We can interact with semaphores using only two operations - wait() and post(). post is sometimes called signal.

wait()
    - tries to decrement the value of the semaphore. 
        - If its greater than zero, then it decrements and returns. 
        - If it is zero, then it waits until the value of semaphore is positive again, then repeat the above.

post()
    - increments the value of the semaphore and returns

USES:
    - Sometimes used like mutex_locks to protect critical shared resource. Although it's not necessary to use semaphore as a mutex lock.
    - Better practice : 
        Do not use semaphores if mutex_locks can be used.

*/