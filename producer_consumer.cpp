#include <iostream>     // For console input and output
#include <thread>       // For Creating and managing threads
#include <mutex>        // For mutual exclusion (locking)
#include <condition_variable>   // For thread signaling (sleep/wake)
#include <queue>        // For the queue data structure
#include <atomic>       

class BoundedBlockingQueue {    // define a class to merge a thread-safe queue
private:
    std::queue<int> buffer;     // the underlying queue to store integer data
    size_t max_capacity;        // max capacity of the buffer
    std::mutex mtx;             // mutex to protect the buffer

    std::condition_variable not_full;   // condn var to signal(to the producer) space is available
    std::condition_variable not_empty;  // condn var to signal(to the consumer) data is available

    std::atomic<bool> is_shutdown{false};      // ensures thread-safe reading/writing without a mutex

public:
    BoundedBlockingQueue(size_t capacity) : max_capacity(capacity) {}; // constructor to set capacity

    void shut_down() {
        is_shutdown = true;
        // CRITICAL: Wake up EVERYONE (producers and consumers)
        // so they can see the 'done' flag and exit
        not_full.notify_all();
        not_empty.notify_all();
    }

    bool push(int val) {
        std::unique_lock<std::mutex> lock(mtx);     // acquire lock for critical section

        while(buffer.size() == max_capacity && !is_shutdown) {      // check if the queue is currently full
            not_full.wait(lock);                    // release the lock and put the producer thread to sleep
        }                                           // Re-acquires the lock automatically after waking up

        if(is_shutdown) return false;
        buffer.push(val);                           // add integer to the back of the queue
        std::cout << "[Producer] Pushed : " << val << " | Size: " << buffer.size() << std::endl;   // log the entered data
        
        not_empty.notify_one();                     // notify exactly one sleeping consumer that data ready for consumption
        return true;
    }                                               // Lock is automatically released (RAII)

    bool pop(int& output_val) {
        std::unique_lock<std::mutex> lock(mtx);     // acquire lock for critical section

        while(buffer.empty() && !is_shutdown) {                     // check if the queue is currently empty
            not_empty.wait(lock);                   // release the lock and put the consumer thread to sleep
        }                                           // Re-acquires the lock automatically after waking up

        if(buffer.empty() && is_shutdown) return false;

        output_val = buffer.front();
        buffer.pop();
        std::cout << "[Consumer] : Consumed " << output_val << " | Size : " << buffer.size() << std::endl;  // log the consumed data

        not_full.notify_one();                      // notify exactly one sleeping producer - space available for new data
        return true;
    }                                               // Lock is automatically release (RAII)
};

// Function for producer task
void producerTask(BoundedBlockingQueue* q) {        // Takes a pointer to the shared queue
    for(int i=1; i<=30; ++i) {                      //produce 10 times(like 10 customers in a coffee shop of only 5 tables)
        if(!q->push(i)) {                                 // send a customer to the table
            std::cout << "[Producer] Shutdown detected. Stopping production." << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::cout << "[Producer] Finised all items" << std::endl;   // poison pill - to terminate if mismatch betn producer and consumer
    q->shut_down();
}

void consumerTask(BoundedBlockingQueue* q) {        // Takes a pointer to the shared queue
    for(int i=1; i<=1000; ++i) {                      // loop to consume(serve) 10 times(customers)
        int val;
        bool pop_status = q->pop(val);                       // exit the loop if production stops
        if(!pop_status) break;                      // Exit if system shut down

        std::this_thread::sleep_for(std::chrono::milliseconds(100));    // sleep to simulate slow processing
    }
    std::cout << "[Consumer] Finished my requirements! Triggering shutdown..." << std::endl;
    q->shut_down();
}

int main() {
    BoundedBlockingQueue queue(5);                  // create a queue instance with capacity of 5

    std::thread producerThread(producerTask, &queue);   // start the producer thread
    std::thread consumerThread(consumerTask, &queue);   // start the consumer thread

    producerThread.join();
    consumerThread.join();

    std::cout << "All work finished." << std::endl;     // Final log message
    return 0;
}
